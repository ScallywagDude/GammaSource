#!/usr/bin/env python3
"""Include VGUI2 controls accessors in Tracker TUs that actually use them.

The historical code included IVGui/ISurface/ISystem interfaces but missed
Controls.h, which declares the free accessor functions. Avoid global defines.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
changed = []
NEEDS = re.compile(r'\bvgui::(?:system|surface|filesystem|ivgui|scheme)\s*\(')
for p in sorted((ROOT / 'Tracker').rglob('*.cpp')):
    before = p.read_bytes()
    text = before.decode('latin1')
    if not NEEDS.search(text):
        continue
    if re.search(r'#\s*include\s*[<\"](?:vgui_controls/Controls\.h|VGUI_Controls\.h)[>\"]', text, re.I):
        continue
    # Insert after the last include in the initial include group; headers
    # that define vgui -> vgui2 are therefore loaded before Controls.h.
    matches = list(re.finditer(r'(?m)^\s*#include\s*[<\"].*[>\"]\s*$', text))
    if not matches:
        continue
    m = matches[-1]
    text = text[:m.end()] + '\n#include <vgui_controls/Controls.h>' + text[m.end():]
    p.write_bytes(text.encode('latin1'))
    changed.append(p.relative_to(ROOT).as_posix())
print('Tracker VGUI accessor includes added:', len(changed))
for item in changed:
    print(' ', item)
