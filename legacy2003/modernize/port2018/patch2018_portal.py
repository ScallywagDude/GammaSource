"""Bring the Portal-Base portal gun into the GammaSource HL2 game DLLs.

Portal-Base (SonicEraZoR, Source SDK 2013) is "HL2 with the Portal gun": the HL2 game built
with PORTAL defined plus the Portal game code, with fixes for running on HL2 maps. This copies
its portal code over the 2018 tree's (a copy of the originals is kept in
source2018_assets/portal2018_orig) and adds it to server_hl2.vpc / client_hl2.vpc the way
Portal-Base does, keeping GammaSource's own pieces (2003 HUD, weapon stubs, clientmode,
gravity gun) instead of Portal's where both exist.

usage: patch2018_portal.py <portal-base sp/src/game dir>
"""
import os, re, shutil, sys

TREE = r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\game"
PB = sys.argv[1]

for d in ('server/portal', 'client/portal', 'shared/portal'):
    src, dst = os.path.join(PB, d), os.path.join(TREE, d)
    for name in os.listdir(src):
        shutil.copy2(os.path.join(src, name), os.path.join(dst, name))
    print('copied', d)

def block(vpc_path, skip):
    """The Portal $Folder block of a Portal-Base vpc, minus the files we keep our own of."""
    t = open(vpc_path, encoding='latin-1').read()
    i = t.index('$Folder\t"Portal"')
    depth, j = 0, t.index('{', i)
    for k in range(j, len(t)):
        depth += {'{': 1, '}': -1}.get(t[k], 0)
        if depth == 0:
            break
    lines = t[i:k + 1].splitlines()
    keep = [l for l in lines if not any(s.lower() in l.lower() for s in skip)]
    return '\t\t' + '\n'.join(keep).replace('\r', '') + '\n'

def patch(vpc, pbvpc, skip, remove, defines, includes):
    t = open(vpc, encoding='latin-1').read().replace('\r\n', '\n')
    if 'GammaSource: Portal-Base' in t:
        print(vpc, 'already patched')
        return
    for r in remove:
        t, n = re.subn(r'[ \t]*\$File\s+"%s"[^\n]*\n' % re.escape(r), '', t, flags=re.I)
        if not n:
            print('  not found in %s: %s' % (vpc, r))
    t = re.sub(r'(\$PreprocessorDefinitions\s+"[^"]*)"', lambda m: m.group(1) + defines + '"', t, count=1)
    t = re.sub(r'(\$AdditionalIncludeDirectories\s+"[^"]*)"', lambda m: m.group(1) + includes + '"', t, count=1)
    # the Portal block goes in the last $Folder "Source Files" level: before the file's final two braces
    k = t.rstrip().rfind('}')
    k = t.rstrip()[:k].rstrip().rfind('}')
    t = t[:k] + '\t\t// GammaSource: Portal-Base portal gun (modernize/port2018/patch2018_portal.py)\n' + block(pbvpc, skip) + t[k:]
    open(vpc, 'w', encoding='latin-1', newline='\n').write(t)
    print('patched', vpc)

patch(os.path.join(TREE, 'server', 'server_hl2.vpc'), os.path.join(PB, 'server', 'server_hl2.vpc'),
      skip=[],
      remove=[r'hl2\hl2_client.cpp', r'hl2\weapon_physcannon.cpp', r'hl2\weapon_physcannon.h',
              r'$SRCDIR\game\shared\hl2\hl2_usermessages.cpp'],
      defines=';PORTAL;GAMMA_PORTALGUN', includes=r';.\portal;$SRCDIR\game\shared\portal')

patch(os.path.join(TREE, 'client', 'client_hl2.vpc'), os.path.join(PB, 'client', 'client_hl2.vpc'),
      # our 2003 quick info and our weapon stubs/clientmode stay
      skip=[r'portal\hud_quickinfo.cpp', r'portal\c_weapon_stubs_portal.cpp', r'portal\clientmode_portal'],
      remove=[r'hl2\c_weapon_physcannon.cpp', r'$SRCDIR\game\shared\hl2\hl2_usermessages.cpp'],
      defines=';PORTAL;GAMMA_PORTALGUN', includes=r';.\portal;.\portal\vgui;$SRCDIR\game\shared\portal')
