"""weapon_vortexhopwire: the 2018 episodic (HL2: Aftermath) vortex hopwire, renamed so it sits next to
the 2003 weapon_hopwire, and using the same models as ours (v_hopwire / w_hopwire)."""
from pathlib import Path

TREE = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\game")

RENAMES = [
    ('grenade_hopwire.h', 'grenade_vortexhopwire.h'),
    ('CGrenadeHopwire', 'CGrenadeVortexHopwire'),
    ('C_GrenadeHopwire', 'C_GrenadeVortexHopwire'),
    ('DT_GrenadeHopwire', 'DT_GrenadeVortexHopwire'),
    ('npc_grenade_hopwire', 'npc_grenade_vortexhopwire'),
    ('CWeaponHopwire', 'CWeaponVortexHopwire'),
    ('DT_WeaponHopwire', 'DT_WeaponVortexHopwire'),
    ('weapon_hopwire', 'weapon_vortexhopwire'),
    ('C_HopwireExplosion', 'C_VortexHopwireExplosion'),
    ('CGravityVortexController', 'CGravityVortexController'),
    ('HopWire_Create', 'VortexHopWire_Create'),
    # our hopwire's grenade model instead of the rollermine
    ('"models/roller.mdl"', '"models/Weapons/w_hopwire.mdl"'),
    ('"models/roller_spikes.mdl"', '"models/Weapons/w_hopwire.mdl"'),
]

FILES = [
    ('server/episodic/grenade_hopwire.h', 'server/gamma/grenade_vortexhopwire.h'),
    ('server/episodic/grenade_hopwire.cpp', 'server/gamma/grenade_vortexhopwire.cpp'),
    ('server/episodic/weapon_hopwire.cpp', 'server/gamma/weapon_vortexhopwire.cpp'),
    ('client/episodic/c_weapon_hopwire.cpp', 'client/gamma/c_weapon_vortexhopwire.cpp'),
]

for src, dst in FILES:
    t = (TREE / src).read_text(encoding='latin-1')
    for old, new in RENAMES:
        t = t.replace(old, new)
    t = '// GammaSource: generated from %s by modernize/port2018/make_vortexhopwire.py\n' % src + t
    (TREE / dst).write_text(t, encoding='latin-1')
    print('wrote', dst)
