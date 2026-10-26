"""The unlabelled w_icons1b sheet has its SMG rows swapped relative to the labelled
w_icons1 sheet (1b 128,128 is the boxy SMG2, 1b 128,192 is the MP5 SMG1), and w_icons2b
128,192 is a different gun from the labelled MANIPULATOR. Pair each weapon's two icon
states with the same art."""
import re
from pathlib import Path
root = Path(r"C:\Users\doge6\Videos\GammaSource-main\gammasource_content\scripts")
FIX = {
    'weapon_smg1':       (('sprites/w_icons1', 129, 129, 126, 62), ('sprites/w_icons1b', 128, 192, 128, 64)),
    'weapon_smg2':       (('sprites/w_icons1', 129, 193, 126, 62), ('sprites/w_icons1b', 128, 128, 128, 64)),
    'weapon_physcannon': (('sprites/w_icons2', 129, 193, 126, 62), ('sprites/w_icons4b', 128, 128, 128, 64)),
}
def render(name, spec, nl):
    f, x, y, w, h = spec
    return (f'"{name}"{nl}\t\t{{{nl}\t\t\t\t"file"\t\t"{f}"{nl}\t\t\t\t"x"\t\t"{x}"{nl}'
            f'\t\t\t\t"y"\t\t"{y}"{nl}\t\t\t\t"width"\t\t"{w}"{nl}\t\t\t\t"height"\t"{h}"{nl}\t\t}}')
for stem, (inactive, active) in FIX.items():
    p = root / (stem + '.txt')
    text = p.read_bytes().decode('latin-1')
    nl = '\r\n' if '\r\n' in text else '\n'
    for role, spec in (('weapon', inactive), ('weapon_s', active)):
        pat = re.compile(r'"%s"\s*\{[^{}]*\}' % re.escape(role))
        assert len(pat.findall(text)) == 1, (stem, role)
        text = pat.sub(lambda m: render(role, spec, nl), text, count=1)
    p.write_bytes(text.encode('latin-1'))
    print('fixed', stem)
