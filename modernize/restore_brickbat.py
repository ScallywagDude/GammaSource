#!/usr/bin/env python3
"""Restore the supplied three-type throw/recover brickbat prototype."""
from pathlib import Path
import re
from restore_hopwire_combat import method
ROOT=Path(__file__).resolve().parents[1]

def weapon(source):
    if 'HL2MODERN_BRICKBAT_INVENTORY' in source:return source
    source=source.replace('//LINK_ENTITY_TO_CLASS( weapon_brickbat, CWeaponBrickbat );','LINK_ENTITY_TO_CLASS( weapon_brickbat, CWeaponBrickbat );').replace('//PRECACHE_WEAPON_REGISTER(weapon_brickbat);','PRECACHE_WEAPON_REGISTER(weapon_brickbat);')
    source=source.replace('UTIL_PrecacheOther("grenade_molotov");','for (int type=0;type<NUM_BRICKBAT_AMMO_TYPES;++type)\n\t\tUTIL_PrecacheOther(BrickBatAmmoArray[type].m_sClassName);')
    source=method(source,'void CWeaponBrickbat::Spawn( void )','''
    m_iCurrentAmmoType=ValidAmmoType();
    m_bNeedDraw=true; m_bNeedThrow=false;
    for (int i=0;i<NUM_BRICKBAT_AMMO_TYPES;++i) m_nAmmoCount[i]=0;
    m_nAmmoCount[m_iCurrentAmmoType]=1;
    BaseClass::Spawn();
''')
    source=source.replace('BrickBatAmmoArray[m_iCurrentAmmoType].m_sViewModel','BrickBatAmmoArray[ValidAmmoType()].m_sViewModel').replace('BrickBatAmmoArray[m_iCurrentAmmoType].m_sWorldModel','BrickBatAmmoArray[ValidAmmoType()].m_sWorldModel')
    source=method(source,'void CWeaponBrickbat::BrickbatTouch( CBaseEntity *pOther )','''
    CBasePlayer *player=ToBasePlayer(pOther);
    if (!player || GetOwner() || IsMarkedForDeletion() ||
        !player->Weapon_CanUse(this) || !g_pGameRules->CanHavePlayerItem(player,this)) return;
    CWeaponBrickbat *owned=dynamic_cast<CWeaponBrickbat *>(player->Weapon_OwnsThisType(GetClassname()));
    if (!owned || owned==this) { BaseClass::DefaultTouch(pOther); return; }
    bool transferred=false,remaining=false;
    for (int type=0;type<NUM_BRICKBAT_AMMO_TYPES;++type)
    {
        int available=max(0,BrickBatAmmoArray[type].m_nMaxCarry-owned->m_nAmmoCount[type]);
        int count=min(available,m_nAmmoCount[type]);
        if (count>0) { owned->m_nAmmoCount[type]+=count; m_nAmmoCount[type]-=count; transferred=true; }
        if (m_nAmmoCount[type]>0) remaining=true;
    }
    if (transferred) owned->SynchronizeAmmo();
    if (!remaining) UTIL_Remove(this);
''')
    source=source.replace('CAI_BaseNPC *pNPC = GetOwner()->MyNPCPointer();','CAI_BaseNPC *pNPC = GetOwner() ? GetOwner()->MyNPCPointer() : NULL;',1)
    source=source.replace('ThrowBrickbat( launchPos, m_vecTossVelocity, sk_npc_dmg_brickbat.GetFloat());','if (!ThrowBrickbat( launchPos, m_vecTossVelocity, sk_npc_dmg_brickbat.GetFloat())) return;',1)
    source=source.replace('CAI_BaseNPC *pNPC\t= GetOwner()->MyNPCPointer();\n\t\tCBaseEntity *pEnemy = pNPC->GetEnemy();','CAI_BaseNPC *pNPC = GetOwner() ? GetOwner()->MyNPCPointer() : NULL;\n\t\tCBaseEntity *pEnemy = pNPC ? pNPC->GetEnemy() : NULL;',1)
    source=source.replace('void CWeaponBrickbat::ThrowBrickbat(', 'bool CWeaponBrickbat::ThrowBrickbat(',1)
    source=source.replace('bool CWeaponBrickbat::ThrowBrickbat( Vector vecSrc, Vector vecVelocity, float damage)\n{','bool CWeaponBrickbat::ThrowBrickbat( Vector vecSrc, Vector vecVelocity, float damage)\n{\n\tm_iCurrentAmmoType=ValidAmmoType();\n\tif (!GetOwner() || m_nAmmoCount[m_iCurrentAmmoType]<=0) return false;',1)
    source=source.replace('if (!pBrickbat)\n\t{','if (!pBrickbat || pBrickbat->IsMarkedForDeletion())\n\t{',1)
    source=source.replace('Msg("Brickbat type (%s) not defined!\\n",BrickBatAmmoArray[m_iCurrentAmmoType].m_sClassName);\n\t\treturn;','Msg("Brickbat type (%s) not defined!\\n",BrickBatAmmoArray[m_iCurrentAmmoType].m_sClassName);\n\t\treturn false;',1)
    source=source.replace('random->RandomFloat ( -100, -500 )','random->RandomFloat ( -500, -100 )')
    source=source.replace('m_nAmmoCount[m_iCurrentAmmoType]--;\n\n\tm_bNeedThrow = false;','m_nAmmoCount[m_iCurrentAmmoType]--;\n\tSynchronizeAmmo();\n\tm_bNeedThrow = false;\n\treturn true;',1)
    source=source.replace('if (!pPlayer)\n\t{\n\t\treturn;\n\t}\n\n\tSendWeaponAnim(ACT_VM_PULLBACK);','if (!pPlayer || !pPlayer->IsAlive() || m_bNeedThrow ||\n\t    m_nAmmoCount[ValidAmmoType()]<=0 || m_flNextPrimaryAttack>gpGlobals->curtime) return;\n\n\tSendWeaponAnim(ACT_VM_PULLBACK);',1)
    source=source.replace('ThrowBrickbat(vecSrc, vecAiming*800, sk_plr_dmg_brickbat.GetFloat());\n\tpPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );','if (!ThrowBrickbat(vecSrc, vecAiming*800, sk_plr_dmg_brickbat.GetFloat()))\n\t{\n\t\tm_bNeedThrow=false;\n\t\tm_bNeedDraw=true;\n\t\tm_flNextPrimaryAttack=gpGlobals->curtime+0.2f;\n\t\treturn;\n\t}',1)
    source=source.replace('CBasePlayer *pOwner = ToBasePlayer( GetOwner() );\n\n\t// -------------------------------------------','CBasePlayer *pOwner = ToBasePlayer( GetOwner() );\n\tif (!pOwner) return;\n\tm_iCurrentAmmoType=ValidAmmoType();\n\n\t// -------------------------------------------',1)
    source=source.replace('int counter = 0;\n\twhile (counter < NUM_BRICKBAT_AMMO_TYPES)','m_iCurrentAmmoType=ValidAmmoType();\n\tint counter = 0;\n\twhile (counter < NUM_BRICKBAT_AMMO_TYPES)',1)
    source=method(source,'void CWeaponBrickbat::ItemPostFrame( void )','''
    CBasePlayer *owner=ToBasePlayer(GetOwner());
    if (!owner || !owner->IsAlive()) { m_bNeedThrow=false; return; }
    SynchronizeAmmo();
    if (m_bNeedThrow)
    {
        if (m_flNextPrimaryAttack<=gpGlobals->curtime) Throw();
        return;
    }
    if (m_bNeedDraw && m_flNextPrimaryAttack<=gpGlobals->curtime) { DrawAmmo(); return; }
    if ((owner->m_nButtons&IN_ATTACK2) && m_flNextSecondaryAttack<=gpGlobals->curtime) SecondaryAttack();
    else if ((owner->m_nButtons&IN_ATTACK) && m_flNextPrimaryAttack<=gpGlobals->curtime)
    {
        if (m_nAmmoCount[ValidAmmoType()]>0) PrimaryAttack();
        else DrawAmmo();
    }
    else WeaponIdle();
''')
    source=source.replace('CWeaponBrickbat::CWeaponBrickbat( void )\n{','CWeaponBrickbat::CWeaponBrickbat( void )\n{\n\tm_iCurrentAmmoType=BRICKBAT_ROCK;\n\tm_bNeedDraw=true; m_bNeedThrow=false; m_iThrowBits=COND_NONE; m_fNextThrowCheck=0;\n\tm_vecTossVelocity.Init();\n\tfor (int i=0;i<NUM_BRICKBAT_AMMO_TYPES;++i) m_nAmmoCount[i]=0;',1)
    source+='''
// HL2MODERN_BRICKBAT_INVENTORY: preserve the original per-type carry limits.
bool CWeaponBrickbat::HasPrimaryAmmo(void)
{
    for (int i=0;i<NUM_BRICKBAT_AMMO_TYPES;++i) if (m_nAmmoCount[i]>0) return true;
    return false;
}

void CWeaponBrickbat::SynchronizeAmmo(void)
{
    CBaseCombatCharacter *owner=GetOwner();
    if (!owner || m_iPrimaryAmmoType<0) return;
    int total=0;
    for (int i=0;i<NUM_BRICKBAT_AMMO_TYPES;++i) total+=m_nAmmoCount[i];
    int reserve=owner->GetAmmoCount(m_iPrimaryAmmoType);
    if (reserve>total) owner->RemoveAmmo(reserve-total,m_iPrimaryAmmoType);
    else if (reserve<total) owner->GiveAmmo(total-reserve,m_iPrimaryAmmoType);
}

void CWeaponBrickbat::Equip(CBaseCombatCharacter *owner)
{
    BaseClass::Equip(owner);
    SynchronizeAmmo();
}

bool CWeaponBrickbat::Holster(CBaseCombatWeapon *switchingTo)
{
    if (!BaseClass::Holster(switchingTo)) return false;
    m_bNeedThrow=false; m_bNeedDraw=true;
    return true;
}

void CWeaponBrickbat::Drop(const Vector &velocity)
{
    m_bNeedThrow=false; m_bNeedDraw=true;
    BaseClass::Drop(velocity);
}
'''
    return source

def header(source):
    source=source.replace('void\t\t\t\tThrowBrickbat(', 'bool\t\t\t\tThrowBrickbat(')
    if 'ValidAmmoType()' not in source:
        source=source.replace('CWeaponBrickbat(void);','''CWeaponBrickbat(void);
    int ValidAmmoType() const { return m_iCurrentAmmoType>=0 && m_iCurrentAmmoType<NUM_BRICKBAT_AMMO_TYPES ? m_iCurrentAmmoType : BRICKBAT_ROCK; }
    bool HasPrimaryAmmo(void);
    bool HasAnyAmmo(void) { return HasPrimaryAmmo(); }
    void SynchronizeAmmo(void);
    void Equip(CBaseCombatCharacter *owner);
    bool Holster(CBaseCombatWeapon *switchingTo=NULL);
    void Drop(const Vector &velocity);
''')
    return source

def grenade(source):
    if 'HL2MODERN_BRICKBAT_RECOVERY' in source:return source
    source=source.replace('VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );','VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );',1)
    source=source.replace('\treturn true;\n}', '''    if (!pPhysics)
    {
        SetMoveType(MOVETYPE_FLYGRAVITY);
        SetSolid(SOLID_BBOX);
        SetSize(Vector(-4,-4,-4),Vector(4,4,4));
    }
    return pPhysics!=NULL;
}''',1)
    source=source.replace('void CGrenade_Brickbat::BrickbatTouch( CBaseEntity *pOther )\n{','void CGrenade_Brickbat::BrickbatTouch( CBaseEntity *pOther )\n{\n\tif (!pOther || pOther==GetOwnerEntity() || IsMarkedForDeletion()) return;',1)
    source=source.replace('vNewVel.y = 100;','vNewVel.z = 100;').replace('vNewVel.z = random->RandomInt(-100,100);','vNewVel.y = random->RandomInt(-100,100);')
    source=source.replace('if (vVelocity.Length() < 0.01)\n\t{\n\t\tSpawnBrickbatWeapon();\n\t}', '''// HL2MODERN_BRICKBAT_RECOVERY: only recover once the object has settled.
    IPhysicsObject *physics=VPhysicsGetObject();
    if ((physics && physics->IsAsleep()) || (!physics && (GetFlags()&FL_ONGROUND) && vVelocity.Length()<20))
    {
        SpawnBrickbatWeapon();
        if (IsMarkedForDeletion()) return;
    }''',1)
    source=source.replace('m_nType = BRICKBAT_ROCK;','m_nType = BRICKBAT_ROCK;\n\t\tm_bExplodes=false;',1)
    return source

def grenade_header(source):
    if 'CGrenade_Brickbat()' not in source:
        source=source.replace('DECLARE_CLASS( CGrenade_Brickbat, CBaseGrenade );','DECLARE_CLASS( CGrenade_Brickbat, CBaseGrenade );\n    CGrenade_Brickbat():m_nType((BrickbatAmmo_t)0),m_bExplodes(false),m_bBounceToFlat(false) {}',1)
    return source

def main():
    changes=[('dlls/hl2_dll/weapon_brickbat.cpp',weapon),('dlls/hl2_dll/weapon_brickbat.h',header),('dlls/hl2_dll/grenade_brickbat.cpp',grenade),('dlls/hl2_dll/grenade_brickbat.h',grenade_header)]
    for filename,change in changes:
        path=ROOT/filename;raw=path.read_bytes();source=raw.decode('latin1').replace('\r\n','\n');updated=change(source)
        if source!=updated:path.write_bytes((updated.replace('\n','\r\n') if b'\r\n' in raw else updated).encode('latin1'))
    path=ROOT/'game_shared/hl2/hl2_gamerules.cpp';raw=path.read_bytes();source=raw.decode('latin1')
    # Reconstructed playable tuning: crowbar-level impact, original 5+3+1 carry limits.
    for name,value in [('sk_plr_dmg_brickbat','10'),('sk_npc_dmg_brickbat','10'),('sk_max_brickbat','9')]:
        source=re.sub(r'("'+name+r'",)"0"',r'\g<1>"'+value+'"',source)
    if source.encode('latin1')!=raw:path.write_bytes(source.encode('latin1'))
    from patch_weapon_restoration_v18 import main as refresh
    refresh()
    print('Original brickbat registered with bounded typed pickup/throw/recovery; gameplay validation required.')

if __name__=='__main__':main()
