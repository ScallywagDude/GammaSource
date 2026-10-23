#!/usr/bin/env python3
"""Bring omitted TF2 prototype weapon APIs onto the supplied 2003 interfaces."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
from restore_hopwire_combat import method

ROOT=Path(__file__).resolve().parents[1]


def infiltrator(source):
    source=source.replace('#include "tf_defines.h"','#include "tf_shareddefs.h"')
    source=source.replace('SequenceDuration( m_nSequence )','SequenceDuration()')
    return source


def mortar(source):
    source=source.replace('#include "tf_defines.h"','#include "tf_shareddefs.h"')
    for header in ('smoke_trail.h','terrainmodmgr.h','tf_obj_antimortar.h'):
        source=source.replace('#include "'+header+'"\n','')
    # The active mortar_round.cpp owns these five variables. A second definition
    # in this previously-uncompiled weapon would prevent linking its branch.
    source=re.sub(r'^ConVar\s+weapon_mortar_(?:shell_damage|shell_radius|starburst_damage|starburst_radius|cluster_shells)\([^\n]*\n','',source,flags=re.M)
    source=source.replace('HasWeaponIdleTimeElapsed()', 'HasWeaponIdleTimeElapsed()')
    source=source.replace('SendPropVector( SENDINFO(m_vecMortarAngles), -1, SPROP_COORD )','SendPropQAngles( SENDINFO(m_vecMortarAngles), 13 )')
    source=source.replace('m_hDeployedMortar->m_iRoundType = iRoundType;', 'm_hDeployedMortar->SetRoundType(iRoundType);')
    if 'HL2MODERN_MORTAR_CONTROL' not in source:
        source=source.replace('\tm_bAccuracyUpgraded = false;\n}', '\tm_bAccuracyUpgraded = false;\n\tm_vecMortarOrigin.Init();\n\tm_vecMortarAngles.Init();\n\tm_flFiringPower=0;\n\tm_flFiringAccuracy=0;\n}',1)
        source=source.replace('\tif ( !pPlayer )\n\t\treturn;\n\t// Can\'t attack', '\tif ( !pPlayer || !pPlayer->IsAlive() || m_flNextPrimaryAttack>gpGlobals->curtime )\n\t\treturn;\n\t// Can\'t attack',1)
        source=source.replace('\tif ( m_hDeployedMortar->FireMortar(', '\tif (m_hDeployedMortar->GetBuilder()!=pPlayer) return;\n\tif ( m_hDeployedMortar->FireMortar(',1)
        source=source.replace('\tm_hDeployedMortar->m_hMortarWeapon = this;','\tm_hDeployedMortar->m_hMortarWeapon = this;',1)
        source=method(source,'void CWeaponMortar::AddAssociatedObject( CBaseObject *pObject )','''
    CObjectMortar *mortar=dynamic_cast<CObjectMortar *>(pObject);
    CBaseTFPlayer *player=ToBaseTFPlayer(GetOwner());
    if (!mortar || !player || mortar->GetBuilder()!=player) return;
    DeployMortar(mortar);
    GainedNewTechnology(NULL);
''')
        source+='''
// HL2MODERN_MORTAR_CONTROL: complete the original client firing-panel command.
bool CWeaponMortar::FireControlled(float power,float accuracy,float yaw)
{
    CBaseTFPlayer *player=ToBaseTFPlayer(GetOwner());
    if (!player || !player->IsAlive() || player->GetActiveWeapon()!=this ||
        !m_hDeployedMortar || m_hDeployedMortar->GetBuilder()!=player ||
        m_hDeployedMortar->IsReloading() || m_flNextPrimaryAttack>gpGlobals->curtime ||
        !(power>=0.0f && power<=1.0f) || !(accuracy>=-0.25f && accuracy<=0.25f) ||
        !(yaw>=-36000.0f && yaw<=36000.0f)) return false;
    SetYaw(AngleNormalize(yaw));
    Fire(power,accuracy);
    return true;
}

bool HL2ModernMortarClientCommand(CBaseTFPlayer *player,const char *command)
{
    if (!command || Q_stricmp(command,"mortar")) return false;
    if (!player || engine->Cmd_Argc()!=4) return true;
    CWeaponMortar *weapon=dynamic_cast<CWeaponMortar *>(player->GetActiveWeapon());
    if (!weapon) return true;
    float values[3];
    for (int i=0;i<3;++i)
    {
        const char *argument=engine->Cmd_Argv(i+1);
        char *end=NULL;
        double value=strtod(argument,&end);
        if (!argument[0] || !end || end==argument || end[0] ||
            !(value>=-36000.0 && value<=36000.0)) return true;
        values[i]=(float)value;
    }
    weapon->FireControlled(values[0],values[1],values[2]);
    return true;
}
'''
    source=source.replace('if ( m_hDeployedMortar == NULL )\n\t\treturn;\n\n\t// Make sure we\'ve got the technology',
                          'if ( m_hDeployedMortar == NULL || iRoundType<0 || iRoundType>=MA_LASTAMMOTYPE )\n\t\treturn;\n\n\t// Make sure we\'ve got the technology')
    if 'BEGIN_DATADESC(CWeaponMortar)' not in source:
        fields={'m_bCarried':'FIELD_BOOLEAN','m_bMortarReloading':'FIELD_BOOLEAN','m_hDeployedMortar':'FIELD_EHANDLE','m_vecMortarOrigin':'FIELD_POSITION_VECTOR','m_vecMortarAngles':'FIELD_VECTOR','m_bRangeUpgraded':'FIELD_BOOLEAN','m_bAccuracyUpgraded':'FIELD_BOOLEAN','m_flFiringPower':'FIELD_FLOAT','m_flFiringAccuracy':'FIELD_FLOAT'}
        data='BEGIN_DATADESC(CWeaponMortar)\n'+''.join('    DEFINE_FIELD(CWeaponMortar,'+name+','+kind+'),\n' for name,kind in fields.items())+'END_DATADESC()\n'
        source=source.replace('LINK_ENTITY_TO_CLASS( weapon_mortar, CWeaponMortar );',data+'LINK_ENTITY_TO_CLASS( weapon_mortar, CWeaponMortar );',1)
    return source


def laserrifle(source):
    source=source.replace('"weapon_laserrifle_damage","0"','"weapon_laserrifle_damage","20"').replace('"weapon_laserrifle_range","0"','"weapon_laserrifle_range","1000"')
    source=source.replace('ToBaseTFPlayer( m_hOwner )','ToBaseTFPlayer( GetOwner() )')
    source=source.replace('pPlayer->Weapon_ShootPosition( pPlayer->GetOrigin() )','pPlayer->Weapon_ShootPosition()')
    source=source.replace('float damagefactor = TFGameRules()->WeaponTraceLine','TFGameRules()->WeaponTraceLine')
    source=source.replace('CBaseEntity::Instance(tr.u.ent)','tr.m_pEnt')
    source=source.replace('float flDamage = GetDamage( (tr.endpos - vecSrc).Length(), tr.hitgroup );','float flDamage = max(0.0f,weapon_laserrifle_damage.GetFloat());')
    source=source.replace('\t\t\tflDamage *= damagefactor;  \n','')
    source=source.replace('pEntity->TraceAttack(', 'pEntity->DispatchTraceAttack(')
    source=source.replace('ApplyMultiDamage( pPlayer, pPlayer )','ApplyMultiDamage()')
    source=source.replace('pPlayer->m_iAmmo[m_iPrimaryAmmoType]--;','pPlayer->RemoveAmmo(1,m_iPrimaryAmmoType);')
    source=source.replace('if ( !pPlayer )\n\t\treturn;','if ( !pPlayer || pPlayer->GetAmmoCount(m_iPrimaryAmmoType)<=0 )\n\t\treturn;',1)
    if 'DECLARE_SERVERCLASS();' not in source:
        source=source.replace('public:\t\n', 'public:\t\n\tDECLARE_SERVERCLASS();\n',1)
        needle='LINK_ENTITY_TO_CLASS( weapon_laserrifle, CWeaponLaserRifle );'
        source=source.replace(needle,'IMPLEMENT_SERVERCLASS_ST(CWeaponLaserRifle,DT_WeaponLaserRifle)\nEND_SEND_TABLE()\n'+needle,1)
    return source


def pistols(source):
    source=source.replace('"weapon_pistols_damage","0"','"weapon_pistols_damage","10"').replace('"weapon_pistols_range","0"','"weapon_pistols_range","1000"')
    if 'DECLARE_SERVERCLASS();' not in source:
        source=source.replace('public:\n','public:\n\tDECLARE_SERVERCLASS();\n',1)
        needle='LINK_ENTITY_TO_CLASS( weapon_pistols, CWeaponPistols );'
        source=source.replace(needle,'IMPLEMENT_SERVERCLASS_ST(CWeaponPistols,DT_WeaponPistols)\nEND_SEND_TABLE()\n'+needle,1)
    if 'HL2MODERN_PISTOLS_STICKY' not in source:
        source=source.replace('DECLARE_SERVERCLASS();','DECLARE_SERVERCLASS();\n\tvirtual void Precache(void);',1)
        source=method(source,'void CWeaponPistols::SecondaryAttack( void )','''
    // HL2MODERN_PISTOLS_STICKY: activate the supplied recon grenade.
    CBasePlayer *player=ToBasePlayer(GetOwner());
    if (!player || !player->IsAlive() || IsOwnerEMPed() ||
        m_flNextSecondaryAttack>gpGlobals->curtime || m_iSecondaryAmmoType<0 ||
        player->GetAmmoCount(m_iSecondaryAmmoType)<=0) return;
    Vector forward;
    player->EyeVectors(&forward);
    Vector origin=player->Weapon_ShootPosition();
    trace_t trace;
    UTIL_TraceHull(origin,origin+forward*16,Vector(-4,-4,-4),Vector(4,4,4),
        MASK_SOLID,player,COLLISION_GROUP_NONE,&trace);
    if (trace.startsolid || trace.allsolid) { m_flNextSecondaryAttack=gpGlobals->curtime+0.2f; return; }
    QAngle angles;
    VectorAngles(forward,angles);
    CBaseGrenade *grenade=dynamic_cast<CBaseGrenade *>(CBaseEntity::Create("grenade_stickybomb",trace.endpos,angles,player));
    if (!grenade || grenade->IsMarkedForDeletion()) { m_flNextSecondaryAttack=gpGlobals->curtime+0.2f; return; }
    grenade->SetOwner(player);
    grenade->SetOwnerEntity(player);
    grenade->SetAbsVelocity(forward*1200);
    grenade->ChangeTeam(player->GetTeamNumber());
    player->RemoveAmmo(1,m_iSecondaryAmmoType);
    PlayAttackAnimation(ACT_VM_SECONDARYATTACK);
    m_flNextSecondaryAttack=gpGlobals->curtime+0.5f;
    m_flNextPrimaryAttack=max((float)m_flNextPrimaryAttack,(float)m_flNextSecondaryAttack);
''')
        source+='''
void CWeaponPistols::Precache(void)
{
    BaseClass::Precache();
    UTIL_PrecacheOther("grenade_stickybomb");
    // Older scripts omitted the secondary slot; use the branch's real 3-grenade pool.
    if (m_iSecondaryAmmoType<0) m_iSecondaryAmmoType=GetAmmoDef()->Index("Grenades");
}
'''
    source=source.replace('m_flNextPrimaryAttack=max(m_flNextPrimaryAttack,m_flNextSecondaryAttack);','m_flNextPrimaryAttack=max((float)m_flNextPrimaryAttack,(float)m_flNextSecondaryAttack);')
    source=source.replace('/* && ( m_flNextSecondaryAttack < gpGlobals->curtime )*/','&& ( m_flNextSecondaryAttack <= gpGlobals->curtime )')
    if 'DECLARE_DATADESC();' not in source:
        source=source.replace('DECLARE_SERVERCLASS();','DECLARE_SERVERCLASS();\n\tDECLARE_DATADESC();\n\tCWeaponPistols():m_flSoonestPrimaryAttack(0),m_flLastPrimaryAttack(0) {}',1)
        source=source.replace('LINK_ENTITY_TO_CLASS( weapon_pistols, CWeaponPistols );','''BEGIN_DATADESC(CWeaponPistols)
    DEFINE_FIELD(CWeaponPistols,m_flSoonestPrimaryAttack,FIELD_TIME),
    DEFINE_FIELD(CWeaponPistols,m_flLastPrimaryAttack,FIELD_TIME),
END_DATADESC()
LINK_ENTITY_TO_CLASS( weapon_pistols, CWeaponPistols );''',1)
    return source


def sticky_grenade(source):
    if 'HL2MODERN_RECON_STICKY_LIFECYCLE' in source:return source
    source=source.replace('"grenade_stickybomb_damage","0"','"grenade_stickybomb_damage","100"').replace('"grenade_stickybomb_radius","0"','"grenade_stickybomb_radius","100"')
    source=source.replace('CSprite\t\t*m_pLiveSprite;','CHandle<CSprite> m_pLiveSprite;')
    source=source.replace('void SetTimer( float timer );','void SetTimer( float timer );\n\tvoid UpdateOnRemove(void);',1)
    source=source.replace('BEGIN_DATADESC( CGrenadeStickyBomb )','BEGIN_DATADESC( CGrenadeStickyBomb )\n\tDEFINE_FIELD(CGrenadeStickyBomb,m_pLiveSprite,FIELD_EHANDLE),',1)
    source=source.replace('CGrenadeStickyBomb::CGrenadeStickyBomb()\n{','CGrenadeStickyBomb::CGrenadeStickyBomb()\n{\n\tm_pLiveSprite=NULL;',1)
    source=source.replace('engine->PrecacheModel( "models/weapons/w_grenade.mdl" );','BaseClass::Precache();\n\tengine->PrecacheModel( "models/weapons/w_grenade.mdl" );\n\tengine->PrecacheModel( "sprites/redglow1.vmt" );',1)
    source=source.replace('\tm_pLiveSprite->SetTransparency(', '\tif (m_pLiveSprite)\n\t{\n\tm_pLiveSprite->SetTransparency(',1)
    source=source.replace('\tm_pLiveSprite->SetAttachment( this, 0 );','\tm_pLiveSprite->SetAttachment( this, 0 );\n\t}',1)
    source=source.replace('if ( pOther->IsBSPModel() == false )','if (!pOther || pOther==GetOwnerEntity() || pOther->IsBSPModel() == false)',1)
    source+='''
// HL2MODERN_RECON_STICKY_LIFECYCLE: glow must also leave on non-explosive removal.
void CGrenadeStickyBomb::UpdateOnRemove(void)
{
    if (m_pLiveSprite) { UTIL_Remove(m_pLiveSprite); m_pLiveSprite=NULL; }
    BaseClass::UpdateOnRemove();
}
'''
    return source


def project_sources(project, sources):
    namespace='http://schemas.microsoft.com/developer/msbuild/2003'
    tag=lambda name:'{'+namespace+'}'+name
    ET.register_namespace('',namespace)
    tree=ET.parse(project);doc=tree.getroot()
    key=lambda value:value.replace('\\','/').casefold()
    existing={key(item.get('Include','')) for item in doc.iter(tag('ClCompile')) if item.get('Include')}
    group=next(g for g in doc.findall(tag('ItemGroup')) if g.find(tag('ClCompile')) is not None)
    changed=False
    for source in sources:
        if key(source) in existing:continue
        item=ET.SubElement(group,tag('ClCompile'),{'Include':source})
        for config in ('Debug HL2','Release HL2','Debug HL1','Release HL1','Debug CounterStrike','Release CounterStrike'):
            ET.SubElement(item,tag('ExcludedFromBuild'),{'Condition':"'$(Configuration)|$(Platform)'=='%s|Win32'"%config}).text='true'
        existing.add(key(source));changed=True
    if changed:
        ET.indent(tree,space='  ')
        tree.write(project,encoding='utf-8',xml_declaration=True)


def client_charge(source):
    return source.replace('vgui::Color', 'Color')


def client_mortar(source):
    return source.replace('gpGlobals->curtimeDelta','gpGlobals->frametime')


def mortar_round(source):
    # Previously-zero placeholder tuning follows the supplied rocket defaults;
    # it is a reconstructed playable baseline, not authenticated 2003 balance.
    for name,value in (('shell_damage','100'),('shell_radius','100'),('starburst_damage','20'),('starburst_radius','100'),('cluster_shells','5')):
        source=source.replace('"weapon_mortar_'+name+'","0"','"weapon_mortar_'+name+'","'+value+'"')
    if 'if (!pGrenade) return NULL;' not in source:
        source=source.replace('CMortarRound *pGrenade = (CMortarRound*)CreateEntityByName("mortar_round");','CMortarRound *pGrenade = (CMortarRound*)CreateEntityByName("mortar_round");\n\tif (!pGrenade) return NULL;',1)
    return source


def designator(source):
    # The already-compiled limpet mine owns this shared replicated variable.
    source=re.sub(r'^ConVar\s+weapon_laserdesignator_range[^\n]*', 'extern ConVar weapon_laserdesignator_range;', source,flags=re.M)
    for header in ('smoke_trail.h','effects.h','baseviewmodel.h','tf_obj_aerial_sentry_station.h'):
        source=source.replace('#include "'+header+'"\n','')
    if '#include "beam_shared.h"' not in source:
        source=source.replace('#include "cbase.h"','#include "cbase.h"\n#include "beam_shared.h"',1)
    source=source.replace('pLimpet->m_hOwner->pev == pPlayer->pev','pLimpet->GetOwner() == pPlayer')
    source=source.replace('pLimpet->m_hOwner->pev == m_hOwner->pev','pLimpet->GetOwner() == GetOwner()')
    source=source.replace('m_hOwner','GetOwner()')
    source=source.replace('CBasePlayer *pPlayer = GetOwner();','CBasePlayer *pPlayer = ToBasePlayer(GetOwner());')
    source=source.replace('pPlayer->Weapon_ShootPosition( pPlayer->GetOrigin() )','pPlayer->Weapon_ShootPosition()')
    source=source.replace('CBaseEntity::Instance( tr.u.ent )','tr.m_pEnt')
    source=source.replace('ENTINDEX(GetOwner()->edict())','GetOwner()->entindex()')
    source=source.replace('m_pBeam->PointEntInit( vec3_origin, iIndex )','m_pBeam->PointEntInit( vec3_origin, pPlayer )').replace('m_pBeam->SetStartEntity( GetOwner()->entindex() )','m_pBeam->SetStartEntity( GetOwner() )')
    source=source.replace('\t\tint iIndex = pPlayer->entindex();\n','')
    source=source.replace('pPlayer->m_aObjects.Size()','pPlayer->GetObjectCount()').replace('pPlayer->m_aObjects[i]','pPlayer->GetObject(i)')
    # The supplied branch has no aerial sentry station class/registration.
    # Its real sentry gun path remains the designation target.
    source=re.sub(r'\n\t\tif \( pObj->GetType\(\) == OBJ_AERIAL_SENTRY_STATION \)\n\t\t\{.*?\n\t\t\}', '', source,flags=re.S)
    source=source.replace('pTarget->Center()', 'pTarget->WorldSpaceCenter()').replace('pPlayer->Center()','pPlayer->WorldSpaceCenter()')
    source=source.replace('pEntity->GetFlags() & FL_NPC','pEntity->GetFlags() & FL_NPC')
    source=source.replace('MASK_SHOT, edict(), COLLISION_GROUP_NONE','MASK_SHOT, this, COLLISION_GROUP_NONE')
    source=source.replace('tr.u.ent != pTarget->edict()','tr.m_pEnt != pTarget')
    source=source.replace('\tASSERT( m_pBeam );','\tif (!m_pBeam) { StopDesignating(); return; }')
    creation='\t\tm_pBeam = CBeam::BeamCreate( "sprites/laserbeam.vmt", 1 );'
    guard='\n\t\tif (!m_pBeam) { m_bDesignating=false; return; }'
    source=re.sub(r'(\n\t\tif \(!m_pBeam\) \{ m_bDesignating=false; return; \})+', '',source)
    source=source.replace(creation,creation+guard)
    if 'DECLARE_SERVERCLASS();' not in source:
        source=source.replace('public:\t\n','public:\t\n\tDECLARE_SERVERCLASS();\n\tDECLARE_DATADESC();\n\tCWeaponLaserDesignator():m_bDesignating(false) {}\n\tvoid UpdateOnRemove(void) { StopDesignating(); BaseClass::UpdateOnRemove(); }\n',1)
        source=re.sub(r'CBeam\s+\*m_pBeam;', 'CHandle<CBeam> m_pBeam;', source)
        needle='LINK_ENTITY_TO_CLASS( weapon_laserdesignator, CWeaponLaserDesignator );'
        source=source.replace(needle,'''IMPLEMENT_SERVERCLASS_ST(CWeaponLaserDesignator,DT_WeaponLaserDesignator)
END_SEND_TABLE()
BEGIN_DATADESC(CWeaponLaserDesignator)
    DEFINE_FIELD(CWeaponLaserDesignator,m_bDesignating,FIELD_BOOLEAN),
    DEFINE_FIELD(CWeaponLaserDesignator,m_pBeam,FIELD_EHANDLE),
END_DATADESC()
'''+needle,1)
    if 'HL2MODERN_DESIGNATOR_LIFECYCLE' not in source:
        source=source.replace('void UpdateOnRemove(void) { StopDesignating(); BaseClass::UpdateOnRemove(); }',
            'void UpdateOnRemove(void) { StopDesignating(); BaseClass::UpdateOnRemove(); }\n\t// HL2MODERN_DESIGNATOR_LIFECYCLE\n\tvoid Drop(const Vector &velocity) { StopDesignating(); BaseClass::Drop(velocity); }')
        source=source.replace('if (pPlayer == NULL)\n\t\treturn;',
            'if (!pPlayer || !pPlayer->IsAlive() || IsOwnerEMPed())\n\t{\n\t\tStopDesignating();\n\t\treturn;\n\t}',1)
        source=method(source,'bool CWeaponLaserDesignator::Holster( CBaseCombatWeapon *pSwitchingTo )','''
    if (!BaseClass::Holster(pSwitchingTo)) return false;
    StopDesignating();
    return true;
''')
        source=source.replace('if ( !pPlayer )\n\t\t\treturn;','if ( !pPlayer ) { StopDesignating(); return; }',1)
    return source


def chargeable(source):
    source=source.replace('public:\n\tDECLARE_SERVERCLASS();','public:\n\tDECLARE_SERVERCLASS();\n\tDECLARE_DATADESC();',1) if 'DECLARE_DATADESC();' not in source else source
    if 'BEGIN_DATADESC(CWeaponCombat_ChargeablePlasma)' not in source:
        needle='LINK_ENTITY_TO_CLASS( weapon_combat_chargeableplasma, CWeaponCombat_ChargeablePlasma );'
        fields={'m_bCharging':'FIELD_BOOLEAN','m_flChargeStartTime':'FIELD_TIME','m_flPower':'FIELD_FLOAT','m_flNextBurstShotTime':'FIELD_TIME','m_iBurstShotsRemaining':'FIELD_INTEGER','m_bHasBurstShot':'FIELD_BOOLEAN','m_bHasCharge':'FIELD_BOOLEAN','m_hLockTarget':'FIELD_EHANDLE','m_vecTargetOffset':'FIELD_VECTOR','m_flLockedAt':'FIELD_TIME'}
        data='BEGIN_DATADESC(CWeaponCombat_ChargeablePlasma)\n'+''.join('    DEFINE_FIELD(CWeaponCombat_ChargeablePlasma,'+name+','+kind+'),\n' for name,kind in fields.items())+'END_DATADESC()\n'
        source=source.replace(needle,data+needle,1)
    if 'HL2MODERN_CHARGE_STATE' not in source:
        source=source.replace('\tm_bHasCharge = false;\n}', '''\tm_bHasCharge = false;
    // HL2MODERN_CHARGE_STATE: initialize every burst/charge/lock field.
    m_bCharging=false;
    m_flChargeStartTime=gpGlobals->curtime;
    m_flPower=0;
    m_flNextBurstShotTime=0;
    m_iBurstShotsRemaining=0;
    m_hLockTarget=NULL;
    m_vecTargetOffset=vec3_origin;
    m_flLockedAt=0;
}''',1)
    source=source.replace('if (!pPlayer)\n\t\treturn;', 'if (!pPlayer || m_iClip1<=0 || IsOwnerEMPed())\n\t\treturn;',1)
    source=source.replace('CGuidedPlasma *pShot = CGuidedPlasma::Create(vecStartSpot, vecAiming, m_hLockTarget, m_vecTargetOffset, pPlayer);',
                          'CGuidedPlasma *pShot = CGuidedPlasma::Create(vecStartSpot, vecAiming, m_hLockTarget, m_vecTargetOffset, pPlayer);\n\tif (!pShot) { m_flNextPrimaryAttack=gpGlobals->curtime+0.15f; return; }') if 'if (!pShot)' not in source else source
    source=source.replace('gpGlobals->curtime - m_flChargeStartTime );','max(0.0f,gpGlobals->curtime - m_flChargeStartTime) );')
    if 'HL2MODERN_PLASMA_FIRE' not in source:
        source=source.replace('\tWeaponSound(SINGLE);\n','',1).replace('\tPlayAttackAnimation( GetPrimaryAttackActivity() );\n','',1)
        source=source.replace('\tif (!pShot) { m_flNextPrimaryAttack=gpGlobals->curtime+0.15f; return; }','\tif (!pShot) { m_flNextPrimaryAttack=gpGlobals->curtime+0.15f; return; }\n\t// HL2MODERN_PLASMA_FIRE: failed creation produces no firing effects.\n\tWeaponSound(SINGLE);\n\tPlayAttackAnimation(GetPrimaryAttackActivity());',1)
        source=method(source,'bool CWeaponCombat_ChargeablePlasma::Holster( CBaseCombatWeapon *pSwitchingTo )','''
    if (!BaseClass::Holster(pSwitchingTo)) return false;
    m_bCharging=false;
    m_iBurstShotsRemaining=0;
    m_hLockTarget=NULL;
    m_flPower=0;
    return true;
''')
        source=source.replace('return SequenceDuration();','return max(0.15f,SequenceDuration());',1)
    return source


def mortar_header(source):
    source=source.replace('bool\t\t\tm_bCarried;', 'CNetworkVar(bool,m_bCarried);').replace('bool\t\t\tm_bMortarReloading;', 'CNetworkVar(bool,m_bMortarReloading);')
    source=source.replace('Vector\t\t\tm_vecMortarOrigin;', 'CNetworkVector(m_vecMortarOrigin);').replace('QAngle\t\t\tm_vecMortarAngles;', 'CNetworkQAngle(m_vecMortarAngles);')
    if 'FireControlled(float' not in source:
        source=source.replace('\tvoid Fire( float flPower, float flAccuracy );','\tvoid Fire( float flPower, float flAccuracy );\n\tbool FireControlled(float power,float accuracy,float yaw);',1)
        source=source.replace('#endif // WEAPON_MORTAR_H','bool HL2ModernMortarClientCommand(CBaseTFPlayer *player,const char *command);\n\n#endif // WEAPON_MORTAR_H',1)
    if 'DECLARE_DATADESC();' not in source:source=source.replace('DECLARE_SERVERCLASS();','DECLARE_SERVERCLASS();\n\tDECLARE_DATADESC();',1)
    return source


def mortar_object_header(source):
    if 'm_hMortarWeapon;' not in source:
        source=source.replace('private:\n', '''    // Original mortar weapon control link, alongside the existing panel.
    CHandle<CWeaponMortar> m_hMortarWeapon;
    void SetRoundType(int type) { if (type>=0 && type<MA_LASTAMMOTYPE) m_iRoundType=type; }
    void UpdateOnRemove(void);
private:
''',1)
    if 'void UpdateOnRemove(void);' not in source:source=source.replace('CHandle<CWeaponMortar> m_hMortarWeapon;','CHandle<CWeaponMortar> m_hMortarWeapon;\n    void UpdateOnRemove(void);',1)
    return source


def mortar_object(source):
    if 'DEFINE_FIELD(CObjectMortar,m_hMortarWeapon,FIELD_EHANDLE)' not in source:
        source=source.replace('BEGIN_DATADESC( CObjectMortar )','BEGIN_DATADESC( CObjectMortar )\n    DEFINE_FIELD(CObjectMortar,m_hMortarWeapon,FIELD_EHANDLE),',1)
    source=source.replace('CMortarRound *pRound = CMortarRound::Create( GetLocalOrigin(), vecTargetVel, edict() );',
                          'CMortarRound *pRound = CMortarRound::Create( GetLocalOrigin(), vecTargetVel, edict() );\n\tif (!pRound) return false;') if 'if (!pRound) return false;' not in source else source
    if 'HL2MODERN_MORTAR_REMOVE' not in source:
        source=source.replace('void CObjectMortar::StartReloading( void )\n{','void CObjectMortar::StartReloading( void )\n{\n\tif (m_hMortarWeapon) m_hMortarWeapon->MortarIsReloading();',1)
        source=source.replace('\tm_iSalvoLeft = MORTAR_SALVO_SIZE;\n}', '\tm_iSalvoLeft = MORTAR_SALVO_SIZE;\n\tif (m_hMortarWeapon) m_hMortarWeapon->MortarFinishedReloading();\n}',1)
        source+='''
// HL2MODERN_MORTAR_REMOVE: neither side retains a destroyed control object.
void CObjectMortar::UpdateOnRemove(void)
{
    if (m_hMortarWeapon && !m_hMortarWeapon->IsMarkedForDeletion())
        m_hMortarWeapon->MortarObjectRemoved();
    m_hMortarWeapon=NULL;
    BaseClass::UpdateOnRemove();
}
'''
    return source


def tf_player(source):
    if '#include "weapon_mortar.h"' not in source:source=source.replace('#include "cbase.h"','#include "cbase.h"\n#include "weapon_mortar.h"',1)
    if 'HL2ModernMortarClientCommand(this,cmd)' not in source:
        source=source.replace('bool CBaseTFPlayer::ClientCommand(const char *cmd)\n{','bool CBaseTFPlayer::ClientCommand(const char *cmd)\n{\n\tif (HL2ModernMortarClientCommand(this,cmd)) return true;',1)
    return source


def main():
    for filename,change in (('game_shared/tf2/weapon_infiltrator.cpp',infiltrator),('game_shared/tf2/weapon_mortar.cpp',mortar),('game_shared/tf2/weapon_laserrifle.cpp',laserrifle),('game_shared/tf2/weapon_pistols.cpp',pistols),('game_shared/tf2/grenade_stickybomb.cpp',sticky_grenade),('game_shared/tf2/weapon_laserdesignator.cpp',designator),('game_shared/tf2/weapon_combat_chargeableplasma.cpp',chargeable),('game_shared/tf2/weapon_mortar.h',mortar_header),('dlls/tf2_dll/tf_obj_mortar.h',mortar_object_header),('dlls/tf2_dll/tf_obj_mortar.cpp',mortar_object),('dlls/tf2_dll/tf_player.cpp',tf_player),('cl_dll/tf2_hud/c_weapon_chargeableplasma.cpp',client_charge),('cl_dll/tf2_hud/c_weapon_mortar.cpp',client_mortar),('dlls/tf2_dll/mortar_round.cpp',mortar_round)):
        path=ROOT/filename
        original=path.read_bytes();source=original.decode('latin1').replace('\r\n','\n');updated=change(source)
        if updated!=source:path.write_bytes((updated.replace('\n','\r\n') if b'\r\n' in original else updated).encode('latin1'))
    for filename in ('tf_guidedplasma.cpp','tf_guidedplasma.h'):
        destination=ROOT/'game_shared/tf2'/filename
        if not destination.exists():destination.write_bytes((ROOT/'modernize/overlays'/filename).read_bytes())
    server=['..\\game_shared\\tf2\\'+name+'.cpp' for name in ('weapon_combat_chargeableplasma','weapon_infiltrator','weapon_laserdesignator','weapon_laserrifle','weapon_mortar','weapon_pistols','tf_guidedplasma')]
    project_sources(ROOT/'dlls/hl.vcxproj',server)
    project_sources(ROOT/'cl_dll/client.vcxproj',['.\\tf2_hud\\c_weapon_chargeableplasma.cpp','.\\tf2_hud\\c_weapon_mortar.cpp'])
    client=ROOT/'cl_dll/tf2_hud/c_weapon__stubs_tf2.cpp'
    raw=client.read_bytes();source=raw.decode('latin1').replace('\r\n','\n');updated=source
    for name,cls,base in (('weapon_infiltrator','WeaponInfiltrator','C_BaseTFCombatWeapon'),('weapon_pistols','WeaponPistols','C_TFMachineGun')):
        if 'STUB_WEAPON_CLASS( '+name+',' not in updated:updated+='\nSTUB_WEAPON_CLASS( '+name+', '+cls+', '+base+' );\n'
    if updated!=source:client.write_bytes((updated.replace('\n','\r\n') if b'\r\n' in raw else updated).encode('latin1'))
    # Refresh the authoritative source/build/grant metadata after new members.
    from patch_weapon_restoration_v18 import main as refresh_inventory
    refresh_inventory()
    print('TF2 prototype API restoration applied; compilation and gameplay checks required.')


if __name__=='__main__':main()
