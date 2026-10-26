"""These weapons borrowed another weapon's selection icon (SLAM, pistol, SMG1). No matching
art exists, so drop the borrowed icon and let the selector show the weapon's name instead."""
import re
from pathlib import Path
root = Path(r"C:\Users\doge6\Videos\GammaSource-main\gammasource_content\scripts")
for stem in ('weapon_thumper', 'weapon_rollerwand', 'weapon_stickylauncher', 'weapon_citizensuitcase', 'weapon_cubemap'):
    p = root / (stem + '.txt')
    text = p.read_bytes().decode('latin-1')
    for role in ('weapon', 'weapon_s'):
        pat = re.compile(r'[ \t]*"%s"\s*\{[^{}]*\}\s*' % re.escape(role))
        assert len(pat.findall(text)) == 1, (stem, role)
        text = pat.sub('', text, count=1)
    p.write_bytes(text.encode('latin-1'))
    print('dropped borrowed icon', stem)
