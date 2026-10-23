#!/usr/bin/env python3
"""Branch-specific impulse 101 weapon grants, derived from registered weapon entities.

The game DLL's branch-specific player override is the only place that receives
these lists. Generic CBasePlayer is not patched with another game's inventory.
No game assets are invented or modified by this script.
"""
from pathlib import Path
import json,re
from weapon_branch_audit import EXCLUDED
root=Path(__file__).resolve().parents[1]
read=lambda rel: (root/rel).read_bytes().decode('latin1')

def patch(rel, changer):
    p=root/rel; raw=p.read_bytes(); crlf=b'\r\n' in raw
    s=raw.decode('latin1').replace('\r\n','\n'); t=changer(s)
    if s!=t:p.write_bytes((t.replace('\n','\r\n') if crlf else t).encode('latin1'))

bases={'weapon_cs_base','weapon_csbase_gun','weapon_basecombatobject','weapon_combat_basegrenade','weapon_combat_grenade','weapon_combat_usedwithshieldbase','weapon_combat_plasmarifle_base'}
# weapon_basecsgrenade is a base class despite its registration.
bases.add('weapon_basecsgrenade')
vanilla={'weapon_frag','weapon_crowbar','weapon_pistol','weapon_ar2','weapon_shotgun','weapon_smg1','weapon_gauss','weapon_physcannon','weapon_rpg'}
regs={}
for name,loc in [('hl2','dlls/hl2_dll'),('cs','game_shared/cstrike'),('tf2','game_shared/tf2')]:
    mapping={}
    for p in sorted((root/loc).glob('*.cpp')):
        txt=p.read_bytes().decode('latin1')
        for m in re.finditer(r'LINK_ENTITY_TO_CLASS\s*\(\s*(weapon_[a-zA-Z0-9_]+)',txt):mapping[m.group(1)]=str(p.relative_to(root))
    regs[name]=mapping

# The HL2 base cheat already grants nine weapons; don't give duplicates.
extra={name: sorted(x for x in mapping if x not in EXCLUDED and (name!='hl2' or x not in vanilla)) for name,mapping in regs.items()}
# Some of these are unusual prototypes and may fail without their assets.
# No cross-game string appears in another branch's emitted grant loop.

def code(names,branch):
    return '''\t\t// HL2MODERN_BRANCH_IMPULSE101_%s: only this branch's concrete registered weapons.\n\t\t// Experimental cut weapons may require missing assets and animations.\n\t\tif (sv_impulse101_allweapons.GetBool())\n\t\t{\n\t\t\tconst char *const weapons[] = {\n%s\n\t\t\t};\n\t\t\tfor (unsigned int k=0; k<sizeof(weapons)/sizeof(weapons[0]); ++k)\n\t\t\t\tGiveNamedItem(weapons[k]);\n\t\t}\n'''%(branch.upper(), '\n'.join('\t\t\t\t"%s",'%w for w in names))

def hl2(s):
    if 'HL2MODERN_BRANCH_IMPULSE101_HL2' in s:return s
    assert 'case 50:' in s
    snippet='''\tcase 101:\n\t{\n\t\t// Preserve the base ammo, suit and standard weapon behavior.\n\t\tBaseClass::CheatImpulseCommands(iImpulse);\n\t\textern int g_iWeaponCheat;\n\t\tif (g_iWeaponCheat != 0)\n\t\t{\n'''+code(extra['hl2'],'hl2')+'''\t\t}\n\t\tbreak;\n\t}\n\n'''
    s=s.replace('\tcase 50:',snippet+'\tcase 50:',1)
    s=s.replace('extern int gEvilImpulse101;', 'extern int gEvilImpulse101;\nextern ConVar sv_impulse101_allweapons;',1)
    return s

def cs(s):
    if 'HL2MODERN_BRANCH_IMPULSE101_CS' in s:return s
    needle='''\t\t\tAddAccount( 16000 );'''; assert needle in s
    s=s.replace(needle,needle+'\n'+code(extra['cs'],'cs'),1)
    # CStrike's own cheat code sets gEvilImpulse101 for this action.
    s=s.replace('void CCSPlayer::CheatImpulseCommands( int iImpulse )', 'extern ConVar sv_impulse101_allweapons;\nvoid CCSPlayer::CheatImpulseCommands( int iImpulse )',1)
    return s

def tf2(s):
    if 'HL2MODERN_BRANCH_IMPULSE101_TF2' in s:return s
    needle='''\t\t\t\tGetPlayerClass()->ResupplyAmmo( 1.0f, RESUPPLY_ALL_FROM_STATION );'''; assert needle in s
    s=s.replace(needle,needle+'\n'+code(extra['tf2'],'tf2'),1)
    s=s.replace('void CBaseTFPlayer::CheatImpulseCommands( int iImpulse )', 'extern ConVar sv_impulse101_allweapons;\nvoid CBaseTFPlayer::CheatImpulseCommands( int iImpulse )',1)
    return s

def base(s):
    if 'HL2MODERN_BRANCH_IMPULSE101_SETTING' in s:return s
    marker='extern int g_iWeaponCheat;'; assert marker in s
    return s.replace(marker,'''// HL2MODERN_BRANCH_IMPULSE101_SETTING: original base cheat gate remains in force.
ConVar sv_impulse101_allweapons("sv_impulse101_allweapons", "1", FCVAR_CHEAT,
    "Include registered experimental weapons in per-branch impulse 101 grants");
'''+marker,1)
patch('dlls/player.cpp',base)
patch('dlls/hl2_dll/hl2_player.cpp',hl2)
patch('dlls/cstrike/cs_player.cpp',cs)
patch('dlls/tf2_dll/tf_player.cpp',tf2)
status=[]
for name in ('hl2','cs','tf2'):
    status.append({'branch':name,'registered':len(regs[name]),'impulse101_extra':extra[name],
         'base_or_abstract_excluded':sorted(set(regs[name]).intersection(bases)),
         'assets_verified':False,'viewmodels_verified':False,'hud_icons_verified':False,
         'texture_scale_verified':False,'windows_build_verified':False})
# HL1's existing cheat is already local to CHL1_Player.
status.append({'branch':'hl1','registered':14,'existing_impulse101_override':True,
    'assets_verified':False,'viewmodels_verified':False,'hud_icons_verified':False,
    'texture_scale_verified':False,'windows_build_verified':False})
# Keep build-aware annotations when this historical registration scanner is
# rerun on a v16+ tree. patch_grants_buildaware.py refreshes these after
# reading the current project inputs; rewriting source registration metadata
# must not silently erase already-audited exclusions.
manifest_path = root/'modernize/weapon_impulse101_manifest.json'
if manifest_path.exists():
    previous={r.get('branch'):r for r in json.loads(manifest_path.read_text())}
    for row in status:
        old=previous.get(row['branch'],{})
        if old.get('impulse101_extra')==row.get('impulse101_extra'):
            for key in ('compiled_impulse101_extra','excluded_not_compiled','note'):
                if key in old:row[key]=old[key]
if manifest_path.exists() and any(x.get('schema_version',0)>=18 for x in json.loads(manifest_path.read_text())):
    # v18 derives registrations from comment-free source and includes shared
    # project members and source/internal exclusions. Do not replace that
    # audited inventory with this historical directory-only regex scan.
    print('Preserving v18 canonical grant manifest; patch_weapon_restoration_v18.py refreshes it.')
else:
    manifest_path.write_text(json.dumps(status,indent=2)+'\n')
print('Weapon branches:',[(x['branch'],x['registered']) for x in status])
