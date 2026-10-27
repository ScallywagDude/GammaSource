"""Point the added weapons' scripts at the selector atlas made by make_icons.ps1 (sprites/w_icons5b)
and at beta-style ammo / crosshair sprites, replacing the retail font-glyph TextureData.

usage: set_icons.py <repo>"""
import os, re, shutil, sys
RETAIL_ROOT = os.environ.get('GAMMA_RETAIL_ROOT', r'C:\GammaSource\retail')  # copies of the retail game folders

REPO = sys.argv[1]
C18 = os.path.join(REPO, 'gammasource_overlay', 'scripts')
BETA = os.path.join(REPO, 'gammasource_content', 'scripts')
RETAIL = os.path.join(RETAIL_ROOT, r'Half-Life 2\hl2\scripts')

CELLS = {  # script -> (atlas x, y, ammo icon donor script or None)
    'weapon_357': (0, 0, 'weapon_pistol'),
    'weapon_ar2future': (128, 0, 'weapon_ar2'),
    'weapon_proto1': (0, 64, 'weapon_pistol'),
    'weapon_portalgun': (128, 64, None),
}

def block(text, key):
    """(start, end) of the {...} body that follows "key"."""
    m = re.search(r'"?%s"?\s*\{' % re.escape(key), text, re.I)
    if not m:
        return None
    depth, i = 0, m.end() - 1
    for k in range(i, len(text)):
        depth += {'{': 1, '}': -1}.get(text[k], 0)
        if depth == 0:
            return m.start(), k + 1
    return None

def sub_block(text, key):
    b = block(text, key)
    return text[b[0]:b[1]] if b else None

for name, (x, y, ammo_from) in CELLS.items():
    dst = os.path.join(C18, name + '.txt')
    if not os.path.exists(dst):
        for d in (BETA, RETAIL):
            if os.path.exists(os.path.join(d, name + '.txt')):
                shutil.copy2(os.path.join(d, name + '.txt'), dst)
                break
    t = open(dst, encoding='latin-1').read()
    icon = ('\t\t"%s"\n\t\t{\n\t\t\t\t"file"\t\t"sprites/w_icons5b"\n\t\t\t\t"x"\t\t"%d"\n\t\t\t\t"y"\t\t"%d"\n'
            '\t\t\t\t"width"\t\t"128"\n\t\t\t\t"height"\t"64"\n\t\t}\n')
    parts = [icon % ('weapon', x, y), icon % ('weapon_s', x, y)]
    if ammo_from:
        donor = open(os.path.join(BETA, ammo_from + '.txt'), encoding='latin-1').read()
        td = sub_block(donor, 'TextureData')
        for k in ('ammo', 'ammo2'):
            b = sub_block(td, k) if td else None
            if b:
                parts.append('\t\t' + b + '\n')
    cross = ('\t\t"crosshair"\n\t\t{\n\t\t\t\t"file"\t\t"sprites/crosshairs"\n\t\t\t\t"x"\t\t\t"0"\n\t\t\t\t"y"\t\t\t"48"\n'
             '\t\t\t\t"width"\t\t"24"\n\t\t\t\t"height"\t"24"\n\t\t}\n')
    parts.append(cross)
    parts.append(cross.replace('"crosshair"', '"autoaim"'))
    new_td = 'TextureData\n\t{\n' + ''.join(parts) + '\t}'
    b = block(t, 'TextureData')
    t = t[:b[0]] + new_td + t[b[1]:] if b else t.rstrip().rstrip('}') + '\n\t' + new_td + '\n}\n'
    if '// GammaSource icons' not in t:
        t = '// GammaSource icons: sprites/w_icons5b (modernize/port2018/make_icons.ps1, set_icons.py)\n' + t
    open(dst, 'w', encoding='latin-1', newline='\r\n').write(t.replace('\r\n', '\n'))
    print('updated', dst)
