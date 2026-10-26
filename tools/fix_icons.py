"""Point weapon selection icons at the right art.
SMG1/SMG2 had each other's beta icons; 357, crossbow and annabelle used
WeaponIcons font glyphs this build lacks, so they get the HL1 icons
(640hud1/2; their glow-style active versions clash with the line-art beta icons)."""
import re
from pathlib import Path
root = Path(r"C:\Users\doge6\Videos\GammaSource-main\gammasource_content\scripts")
FIX = {
    'weapon_smg1':      (('sprites/w_icons1', 128, 128, 128, 64), ('sprites/w_icons1b', 128, 128, 128, 64)),
    'weapon_smg2':      (('sprites/w_icons1', 128, 192, 128, 64), ('sprites/w_icons1b', 128, 192, 128, 64)),
    'weapon_357':       (('sprites/640hud1', 0, 90, 170, 45),     ('sprites/640hud1', 0, 90, 170, 45)),
    'weapon_crossbow':  (('sprites/640hud2', 0, 0, 170, 45),      ('sprites/640hud2', 0, 0, 170, 45)),
    'weapon_annabelle': (('sprites/640hud1', 0, 180, 170, 45),    ('sprites/640hud1', 0, 180, 170, 45)),
}
def render(name, spec, nl):
    f, x, y, w, h = spec
    return (f'\t\t"{name}"{nl}\t\t{{{nl}\t\t\t\t"file"\t\t"{f}"{nl}\t\t\t\t"x"\t\t"{x}"{nl}'
            f'\t\t\t\t"y"\t\t"{y}"{nl}\t\t\t\t"width"\t\t"{w}"{nl}\t\t\t\t"height"\t"{h}"{nl}\t\t}}')
for stem, (inactive, active) in FIX.items():
    p = root / (stem + '.txt')
    raw = p.read_bytes()
    text = raw.decode('latin-1')
    nl = '\r\n' if '\r\n' in text else '\n'
    for role, spec in (('weapon', inactive), ('weapon_s', active)):
        pat = re.compile(r'[ \t]*"%s"\s*\{[^{}]*\}' % re.escape(role))
        assert len(pat.findall(text)) == 1, (stem, role)
        text = pat.sub(lambda m: render(role, spec, nl), text, count=1)
    p.write_bytes(text.encode('latin-1'))
    print('fixed', stem)
