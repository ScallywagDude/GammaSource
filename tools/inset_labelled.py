"""The labelled beta sheets (w_icons1/2/3) have a 1px grid baked around each 128x64 cell,
which drew a box around inactive icons only. Inset those "weapon" rects by 1px."""
import re
from pathlib import Path
root = Path(r"C:\Users\doge6\Videos\GammaSource-main\gammasource_content\scripts")
for p in sorted(root.glob('weapon_*.txt')):
    text = p.read_bytes().decode('latin-1')
    m = re.search(r'"weapon"\s*\{([^{}]*)\}', text)
    if not m or not re.search(r'"file"\s*"sprites/w_icons[123]"', m.group(1)):
        continue
    body = m.group(1)
    def num(k):
        return int(re.search(r'"%s"\s*"(\d+)"' % k, body).group(1))
    if num('width') != 128 or num('height') != 64:
        continue  # already inset
    new = body
    for k, delta in (('x', 1), ('y', 1), ('width', -2), ('height', -2)):
        new = re.sub(r'("%s"\s*")(\d+)(")' % k, lambda mm: mm.group(1) + str(int(mm.group(2)) + delta) + mm.group(3), new, count=1)
    text = text[:m.start(1)] + new + text[m.end(1):]
    p.write_bytes(text.encode('latin-1'))
    print('inset', p.stem)
