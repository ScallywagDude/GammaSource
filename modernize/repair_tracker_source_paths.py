#!/usr/bin/env python3
"""Reconstruct minimal absent Tracker common pieces and VGUI include forwards.

Only repairs source/header-not-found diagnostics from the historical Tracker tree.
Does not certify the old Tracker UI against the 2003 VGUI2 API.
Run after convert_dsp.py so the generated projects retain the include paths.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
COMPAT = ROOT / 'modernize' / 'tracker_vgui_compat'
PROJECT_ROOT = ROOT / 'Tracker'

# These are exact forwarding paths for headers present in this source archive.
# A bridge never defines a fake class, symbol, ABI or function implementation.
OVERRIDES = {'KeyValues.h': 'KeyValues.h', 'Color.h': 'Color.h'}

def forward_target(legacy_name):
    basename = legacy_name[5:]
    for candidate in (OVERRIDES.get(basename), 'vgui_controls/' + basename,
                      'vgui/' + basename):
        if candidate and (ROOT / 'public' / candidate).is_file():
            return candidate
    raise RuntimeError('No real VGUI header for %s' % legacy_name)


def write_exact(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == data:
        return False
    path.write_bytes(data)
    return True


def repair():
    compat_files = set()
    count = 0
    for source in PROJECT_ROOT.rglob('*'):
        if source.suffix.lower() not in ('.cpp', '.h'):
            continue
        for legacy in re.findall(rb'#\s*include\s*[<"](VGUI_[A-Za-z_0-9]+\.h)[>"]',
                                 source.read_bytes(), re.I):
            compat_files.add(legacy.decode('ascii'))
    for legacy in sorted(compat_files):
        target = forward_target(legacy)
        contents = ('// Forward legacy Tracker include spelling to the existing VGUI2 header.\n'
                    '#pragma once\n#include <%s>\n' % target).encode('ascii')
        count += write_exact(COMPAT / legacy, contents)

    # The VC6 projects reference this directory's relative path; preserve
    # the original project metadata and edit only the include-directory text.
    project_count = 0
    for project in PROJECT_ROOT.rglob('*.vcxproj'):
        blob = project.read_bytes()
        text = blob.decode('utf-8')
        # All of these projects are exactly two directories below the root.
        addon = '..\\..\\modernize\\tracker_vgui_compat'
        def add(match):
            val = match.group(1)
            if addon.lower() in val.lower().split(';'):
                return match.group(0)
            return '<AdditionalIncludeDirectories>' + addon + ';' + val + '</AdditionalIncludeDirectories>'
        updated = re.sub(r'<AdditionalIncludeDirectories>(.*?)</AdditionalIncludeDirectories>',
                         add, text)
        if updated != text:
            project.write_bytes(updated.encode('utf-8'))
            project_count += 1
    # The VC6 Tracker DSPs still refer to a few locations that were moved in
    # this archive. Redirect only the generated project references to real
    # implementations; keep the original historical DSP files unchanged.
    moves = {
        r'..\..\common\Random.cpp': r'..\..\common\GameUI\Random.cpp',
        r'..\..\common\MemPool.cpp': r'..\..\public\mempool.cpp',
        r'..\..\public\UtlBuffer.cpp': r'..\..\public\utlbuffer.cpp',
    }
    redirects = 0
    for project in PROJECT_ROOT.rglob('*.vcxproj'):
        for suffix in ('', '.filters'):
            path = Path(str(project) + suffix)
            if not path.exists():
                continue
            before = path.read_bytes().decode('utf-8')
            after = before
            for old, target in moves.items():
                after = after.replace(old, target)
            if after != before:
                path.write_bytes(after.encode('utf-8'))
                redirects += 1
    print('Tracker bridges: %d present (%d written); %d projects updated; %d redirect files' %
          (len(compat_files), count, project_count, redirects))
    return project_count + redirects


if __name__ == '__main__':
    repair()
