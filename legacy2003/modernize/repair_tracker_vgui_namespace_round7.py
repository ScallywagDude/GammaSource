#!/usr/bin/env python3
"""Keep legacy Tracker VGUI accessor declarations in the VGUI2 namespace.

public/vgui/VGUI.h intentionally maps the legacy `vgui` identifier to `vgui2`.
If public/vgui_controls/Controls.h is included first, it declares system(),
ivgui(), filesystem() and surface() in the *wrong* namespace. In affected
translation units, later use of vgui::system() then expands to vgui2::system()
and fails. Include the VGUI mapping before any other project headers for the
Tracker sources which use these accessors. The engine headers and other
projects are left unchanged.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
TRACKER = ROOT / 'Tracker'
LEGACY = ROOT / 'modernize' / 'tracker_vgui_compat' / 'VGUI_Controls.h'
ACCESSOR = re.compile(r'\b(?:vgui::)?(?:system|ivgui|surface|filesystem|scheme|input|ipanel|localize)\s*\(\s*\)')
FIRST_INCLUDE = re.compile(r'(?m)^([ \t]*)#\s*include\s*[<"][^>"\r\n]+[>"]')
CANONICAL = '#include <vgui/VGUI.h>'


def write_if_changed(path, data):
    if path.read_bytes() == data:
        return False
    path.write_bytes(data)
    return True


def repair():
    changes = []
    # The original forwarding shim could run before the #define vgui vgui2.
    # Include complete enums before Controls.h's legacy enum redeclarations
    # to keep this header accepted by compilers which reject opaque C++98 enums.
    shim = ('// Legacy Tracker controls use the VGUI2 namespace macro.\n'
            '#pragma once\n'
            '#include <vgui/VGUI.h>\n'
            '#include <vgui/KeyCode.h>\n'
            '#include <vgui/MouseCode.h>\n'
            '#include <vgui_controls/Controls.h>\n').encode('ascii')
    if write_if_changed(LEGACY, shim):
        changes.append(LEGACY.relative_to(ROOT).as_posix())

    for path in sorted(TRACKER.rglob('*.cpp')):
        raw = path.read_bytes()
        s = raw.decode('latin1')
        if not ACCESSOR.search(s):
            continue
        # Collapse a duplicate top-of-file include from older repair attempts.
        s = re.sub(r'(?m)^(#include <vgui/VGUI\.h>\r?\n){2,}', CANONICAL + ('\r\n' if '\r\n' in s[:2048] else '\n'), s)
        match = FIRST_INCLUDE.search(s)
        if not match:
            raise ValueError('Tracker accessor caller has no include: ' + str(path))
        # Read until the first header; the macro must be present already at
        # this point rather than inserted after headers that included Controls.
        prefix = s[:match.start()]
        if (re.search(r'(?m)^\s*#\s*include\s*<vgui/VGUI\.h>\s*$', prefix)
                or re.match(r'#\s*include\s*<vgui/VGUI\.h>\s*$', match.group().strip())):
            if s.encode('latin1') != raw and write_if_changed(path, s.encode('latin1')):
                changes.append(path.relative_to(ROOT).as_posix())
            continue
        nl = '\r\n' if '\r\n' in s[:2048] else '\n'
        new = s[:match.start()] + CANONICAL + nl + s[match.start():]
        if write_if_changed(path, new.encode('latin1')):
            changes.append(path.relative_to(ROOT).as_posix())

    print('Tracker VGUI namespace repairs changed:', len(changes))
    for change in changes:
        print(' ', change)
    return changes


if __name__ == '__main__':
    repair()
