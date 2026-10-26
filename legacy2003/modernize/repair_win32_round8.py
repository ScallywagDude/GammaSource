#!/usr/bin/env python3
"""Conservative Win32 build fixes based on the October 23 compiler diagnostics.

Run after the original DSP conversion and the earlier repair scripts.
Repeat-safe. Does not create an AMD64 configuration or alter Phase 7.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def replace_once(relative, old, new):
    path = ROOT / relative
    data = path.read_bytes()
    text = data.decode('latin1')
    if new in text:
        return False
    if text.count(old) != 1:
        raise RuntimeError('%s: expected one exact target, found %s' % (relative, text.count(old)))
    path.write_bytes(text.replace(old, new, 1).encode('latin1'))
    return True


def run():
    count = 0
    # IMPLEMENT_PANELMAP() takes &baseClass::m_PanelMap. This requires the
    # base map to be accessible to derived panel classes, not private.
    count += replace_once(
        'Tracker/ServerBrowser/BaseGamesPage.h',
        '\tDECLARE_PANELMAP();\n\ttypedef vgui::PropertyPage BaseClass;',
        '\tprotected: // Derived panel maps require access to the base map.\n\tDECLARE_PANELMAP();\n\ttypedef vgui::PropertyPage BaseClass;')

    # The DSP declares both a staged copy of the font library and its direct
    # build output, causing LNK2005 for every CFontManager definition.
    project = ROOT / 'vguimatsurface/vguimatsurface.vcxproj'
    data = project.read_bytes()
    source = data.decode('utf8')
    changed = re.sub(r'(<AdditionalDependencies>)vgui_surfacelib\.lib;', r'\1', source)
    if changed == source and '<AdditionalDependencies>vgui_surfacelib.lib;' in source:
        raise RuntimeError('Unexpected duplicate font library dependency layout')
    if changed != source:
        project.write_bytes(changed.encode('utf8'))
        count += 1

    # A direct include of <fstream.h> is not supported by current MSVC.
    # The compatibility header aliases standard C++98 streams without
    # changing file IO semantics. VC6 ios::nocreate was redundant for ios::in.
    header = ROOT / 'public/legacy_fstream_compat.h'
    content = '''// Source 2003 / VC6 stream-name compatibility for standard C++98 streams.\n#ifndef HL2_LEGACY_FSTREAM_COMPAT_H\n#define HL2_LEGACY_FSTREAM_COMPAT_H\n#include <fstream>\nusing std::fstream;\nusing std::ifstream;\nusing std::ofstream;\nusing std::ios;\n#endif\n'''
    if not header.exists():
        header.write_text(content)
        count += 1
    elif header.read_text() != content:
        raise RuntimeError('Existing stream compatibility header differs from expected contents')
    paths = list((ROOT / 'worldcraft').rglob('*.h')) + list((ROOT / 'worldcraft').rglob('*.cpp')) + [ROOT/'public/tokenreader.h', ROOT/'public/tokenreader.cpp']
    for path in paths:
        original = path.read_bytes()
        text = original.decode('latin1')
        if path == ROOT/'public/tokenreader.h':
            text = text.replace('#include "fstream.h"', '#include "legacy_fstream_compat.h"')
        else:
            text = text.replace('#include <fstream.h>', '#include "../public/legacy_fstream_compat.h"')
            text = text.replace('#include "fstream.h"', '#include "../public/legacy_fstream_compat.h"')
            if path == ROOT/'worldcraft/keyvalues.h':
                text = text.replace('class fstream;', '#include "../public/legacy_fstream_compat.h"')
        # All usages of nocreate in this tree also request input mode.
        if 'ios::nocreate' in text:
            if re.search(r'\bios::nocreate\b', text) and not re.search(r'\bios::in\b', text) and 'ifstream' not in text:
                raise RuntimeError('No input mode for nocreate in '+str(path))
            text = re.sub(r'\s*\|\s*ios::nocreate\b', '', text)
            text = re.sub(r'\bios::nocreate\s*\|\s*', '', text)
        if text != original.decode('latin1'):
            path.write_bytes(text.encode('latin1'))
            count += 1

    # Legacy x86 binary archives without SAFESEH tables cannot be fixed by
    # rebuilding source that we don't have. Opt-in only: modern linker keeps
    # its safer default until the user explicitly enables this compatibility.
    targets = ROOT / 'Directory.Build.targets'
    targets_content = '''<?xml version="1.0" encoding="utf-8"?>\n<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n  <!-- Opt in ONLY for legacy Win32 archives whose object files lack SAFESEH tables. -->\n  <!-- /SAFESEH:NO reduces exploit mitigation; use source rebuilds with /safeseh when possible. -->\n  <ItemDefinitionGroup Condition="'$(Platform)'=='Win32' and '$(HL2ModernLegacyUnsafeSEH)'=='true'">\n    <Link>\n      <ImageHasSafeExceptionHandlers>false</ImageHasSafeExceptionHandlers>\n    </Link>\n  </ItemDefinitionGroup>\n</Project>\n'''
    if not targets.exists():
        targets.write_text(targets_content)
        count += 1
    elif targets.read_text() != targets_content:
        raise RuntimeError('Existing Directory.Build.targets differs; refuse overwrite')

    return count


if __name__ == '__main__':
    print('Win32 round-8 source repairs: %d files changed' % run())
