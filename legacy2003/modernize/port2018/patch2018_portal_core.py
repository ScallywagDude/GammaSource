"""Small core-file hooks the Portal-Base portal code expects (see patch2018_portal.py).
Idempotent: each edit is skipped when its marker is already present."""
import os, re, shutil, sys

TREE = r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\game"
PB = sys.argv[1]

def edit(rel, old, new, marker):
    p = os.path.join(TREE, rel)
    raw = open(p, encoding='latin-1', newline='').read()
    crlf = '\r\n' in raw
    t = raw.replace('\r\n', '\n')
    if marker in t:
        return
    if old not in t:
        print('  not found in %s: %r' % (rel, old[:60]))
        return
    t = t.replace(old, new, 1)
    open(p, 'w', encoding='latin-1', newline='').write(t.replace('\n', '\r\n') if crlf else t)
    print('patched', rel)

# entity-type queries Portal-Base added
edit(r'server\baseentity.h', '\tvirtual bool\t\t\tIsNPC( void ) const { return false; }',
     '\tvirtual bool\t\t\tIsNPC( void ) const { return false; }\n'
     '\tvirtual bool\tIsBaseTank() const { return false; }	// GammaSource/Portal-Base: true for func_tank', 'IsBaseTank')
edit(r'server\hl2\func_tank.h', '\tDECLARE_DATADESC();',
     '\tDECLARE_DATADESC();\n\tvirtual bool IsBaseTank() const { return true; }	// GammaSource/Portal-Base', 'IsBaseTank')
edit(r'shared\basecombatweapon_shared.h', '\tvirtual bool\t\t\tIsPredicted( void ) const { return false; }',
     '\tvirtual bool\t\t\tIsPredicted( void ) const { return false; }\n'
     '\tvirtual bool\t\t\tIsPortalGun() { return false; }	// GammaSource/Portal-Base: true for weapon_portalgun', 'IsPortalGun')

# Portal's radio silencing needs Portal's achievements, which the HL2 game does not have
edit(r'server\sound.cpp', '#ifdef PORTAL\n\t\t// This is the only way we can silence the radio',
     '#if defined( PORTAL ) && !defined( HL2_DLL )\n\t\t// This is the only way we can silence the radio', 'defined( PORTAL ) && !defined( HL2_DLL )')

# Portal-Base's single-player animation state
for name in ('singleplayer_animstate.cpp', 'singleplayer_animstate.h'):
    shutil.copy2(os.path.join(PB, 'shared', name), os.path.join(TREE, 'shared', name))
for vpc, extra in ((r'server\server_hl2.vpc', ''), (r'client\client_hl2.vpc', '')):
    edit(vpc, '\t\t// GammaSource: Portal-Base portal gun',
         '\t\t$File\t"$SRCDIR\\game\\shared\\singleplayer_animstate.cpp"\n'
         '\t\t$File\t"$SRCDIR\\game\\shared\\singleplayer_animstate.h"\n'
         '\t\t// GammaSource: Portal-Base portal gun', 'singleplayer_animstate')

# Portal-Base's "Mod Options" panel needs its vgui control changes; GammaSource has its own options
p = os.path.join(TREE, r'client\client_hl2.vpc')
t = open(p, encoding='latin-1').read()
t2 = re.sub(r'[ \t]*\$File\s+"portal\\(IModSettingsPanel\.h|ModSettingsPanel\.cpp)"[^\n]*\n', '', t, flags=re.I)
if t2 != t:
    open(p, 'w', encoding='latin-1', newline='').write(t2)
    print('removed ModSettingsPanel from client_hl2.vpc')
