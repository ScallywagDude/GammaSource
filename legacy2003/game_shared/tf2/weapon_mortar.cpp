//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_player.h"
#include "tf_basecombatweapon.h"
#include "tf_shareddefs.h"
#include "in_buttons.h"
#include "gamerules.h"
#include "ammodef.h"
#include "tf_obj.h"
#include "weapon_mortar.h"
#include "tf_shareddefs.h"
#include "tf_team.h"
#include "tf_gamerules.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"

extern short	g_sModelIndexFireball;

// Damage CVars


//=====================================================================================================
// MORTAR WEAPON
//=====================================================================================================
BEGIN_DATADESC(CWeaponMortar)
    DEFINE_FIELD(CWeaponMortar,m_bCarried,FIELD_BOOLEAN),
    DEFINE_FIELD(CWeaponMortar,m_bMortarReloading,FIELD_BOOLEAN),
    DEFINE_FIELD(CWeaponMortar,m_hDeployedMortar,FIELD_EHANDLE),
    DEFINE_FIELD(CWeaponMortar,m_vecMortarOrigin,FIELD_POSITION_VECTOR),
    DEFINE_FIELD(CWeaponMortar,m_vecMortarAngles,FIELD_VECTOR),
    DEFINE_FIELD(CWeaponMortar,m_bRangeUpgraded,FIELD_BOOLEAN),
    DEFINE_FIELD(CWeaponMortar,m_bAccuracyUpgraded,FIELD_BOOLEAN),
    DEFINE_FIELD(CWeaponMortar,m_flFiringPower,FIELD_FLOAT),
    DEFINE_FIELD(CWeaponMortar,m_flFiringAccuracy,FIELD_FLOAT),
END_DATADESC()
LINK_ENTITY_TO_CLASS( weapon_mortar, CWeaponMortar );
PRECACHE_WEAPON_REGISTER(weapon_mortar);

EXTERN_SEND_TABLE(DT_BaseCombatWeapon)

IMPLEMENT_SERVERCLASS_ST(CWeaponMortar, DT_WeaponMortar)
	SendPropInt( SENDINFO( m_bCarried ), 2, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bMortarReloading ), 2, SPROP_UNSIGNED ),
	SendPropVector( SENDINFO(m_vecMortarOrigin), -1, SPROP_COORD ),
	SendPropQAngles( SENDINFO(m_vecMortarAngles), 13 ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponMortar::CWeaponMortar( void )
{
#ifdef _DEBUG
	m_vecMortarOrigin.Init();
	m_vecMortarAngles.Init();
#endif

	m_bCarried = true;
	m_bMortarReloading = false;
	m_hDeployedMortar = NULL;
	m_bRangeUpgraded = false;
	m_bAccuracyUpgraded = false;
	m_vecMortarOrigin.Init();
	m_vecMortarAngles.Init();
	m_flFiringPower=0;
	m_flFiringAccuracy=0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponMortar::GetFireRate( void )
{	
	return 3.0; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponMortar::Deploy( )
{
	return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_SLAM_TRIPMINE_DRAW, (char*)GetAnimPrefix() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMortar::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (pPlayer == NULL)
		return;

	if ( pPlayer->m_nButtons & IN_ATTACK )
	{
		if ( m_bCarried )
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "\n\n\n\n\n\n\n\n\n\nBuild your mortar with the build weapon first!" );
		}
	}
	else if ( pPlayer->m_nButtons & IN_ATTACK2 )
	{
		SecondaryAttack();
	}

	// No buttons down.
	if (!(( pPlayer->m_nButtons & IN_ATTACK ) || ( pPlayer->m_nButtons & IN_ATTACK2 )))
	{
		WeaponIdle();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponMortar::ComputeEMPFireState( void )
{
	if (IsOwnerEMPed())
	{
		CPASAttenuationFilter filter( GetOwner(), "WeaponMortar.EMPed" );
		EmitSound( filter, GetOwner()->entindex(), "WeaponMortar.EMPed" );
		return false;
	}
	return true;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMortar::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer || !pPlayer->IsAlive() || m_flNextPrimaryAttack>gpGlobals->curtime )
		return;
	// Can't attack if taking EMP damage
	if ( !ComputeEMPFireState() )
		return;
	if ( IsOwnerEMPed() )
		return;
	if ( m_hDeployedMortar == NULL )
		return;

	if (m_hDeployedMortar->GetBuilder()!=pPlayer) return;
	if ( m_hDeployedMortar->FireMortar( m_flFiringPower, m_flFiringAccuracy, m_bRangeUpgraded, m_bAccuracyUpgraded ) )
	{
		WeaponSound( SINGLE );
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;

	CheckRemoveDisguise();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMortar::Fire( float flPower, float flAccuracy )
{
	m_flFiringPower = flPower;
	m_flFiringAccuracy = flAccuracy;

	PrimaryAttack();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMortar::SecondaryAttack( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	// Setup for refire
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: Player's finished deploying his mortar
//-----------------------------------------------------------------------------
void CWeaponMortar::DeployMortar( CObjectMortar *pMortar )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	ClientPrint( pPlayer, HUD_PRINTCENTER, "\n\n\n\n\n\n\n\n\n\nMortar Deployed" );

	m_bCarried = false;
	m_hDeployedMortar = pMortar;
	m_hDeployedMortar->m_hMortarWeapon = this;
	SendWeaponAnim( ACT_SLAM_DETONATOR_DRAW );

	m_vecMortarOrigin = m_hDeployedMortar->GetLocalOrigin();
	m_vecMortarAngles = m_hDeployedMortar->GetLocalAngles();
}

//-----------------------------------------------------------------------------
// Purpose: Mortar object has been removed
//-----------------------------------------------------------------------------
void CWeaponMortar::MortarObjectRemoved( void )
{
	m_hDeployedMortar = NULL;
	m_bCarried = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMortar::SetYaw( float flYaw )
{
	if ( m_hDeployedMortar == NULL )
		return;

	QAngle angles = m_hDeployedMortar->GetLocalAngles();
	angles.y = flYaw;
	m_hDeployedMortar->SetLocalAngles( angles );
	m_vecMortarAngles = angles;
}

//-----------------------------------------------------------------------------
// Purpose: Set the deployed mortar's firing round
//-----------------------------------------------------------------------------
void CWeaponMortar::SetRoundType( int iRoundType )
{
	if ( m_hDeployedMortar == NULL || iRoundType<0 || iRoundType>=MA_LASTAMMOTYPE )
		return;

	// Make sure we've got the technology for this round type
	if ( MortarAmmoTechs[ iRoundType ] && MortarAmmoTechs[ iRoundType ][0] )
	{
		CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
		if ( !pPlayer )
			return;
		// Does the player have the technology?
		if ( pPlayer->HasNamedTechnology( MortarAmmoTechs[ iRoundType ] ) == false )
			return;
	}

	m_hDeployedMortar->SetRoundType(iRoundType);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMortar::MortarDestroyed( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( pPlayer )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "\n\n\n\n\n\n\n\n\n\nMortar Destroyed!" );
	}

	if ( pPlayer && pPlayer->GetActiveWeapon() == this )
	{
		SendWeaponAnim( ACT_SLAM_TRIPMINE_DRAW );
	}

	MortarObjectRemoved();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CWeaponMortar::AddAssociatedObject( CBaseObject *pObject )
{
    CObjectMortar *mortar=dynamic_cast<CObjectMortar *>(pObject);
    CBaseTFPlayer *player=ToBaseTFPlayer(GetOwner());
    if (!mortar || !player || mortar->GetBuilder()!=player) return;
    DeployMortar(mortar);
    GainedNewTechnology(NULL);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CWeaponMortar::RemoveAssociatedObject( CBaseObject *pObject )
{
	Assert( pObject );

	// Can't handle this object
	CObjectMortar *mortar = dynamic_cast< CObjectMortar * >( pObject );
	if ( !mortar )
		return;

	if ( m_hDeployedMortar == mortar )
	{
		m_hDeployedMortar = NULL;
		m_bCarried = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: The player holding this weapon has just gained new technology.
//			Check to see if it affects the mortar
//-----------------------------------------------------------------------------
void CWeaponMortar::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( pPlayer )
	{
		// Range upgraded?
		if ( pPlayer->HasNamedTechnology("mortar_range") )
			m_bRangeUpgraded = true;
		else
			m_bRangeUpgraded = false;

		// Accuracy upgraded?
		if ( pPlayer->HasNamedTechnology("mortar_accuracy") )
			m_bAccuracyUpgraded = true;
		else
			m_bAccuracyUpgraded = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponMortar::WeaponIdle( void )
{
	if ( HasWeaponIdleTimeElapsed() )
	{
		if ( m_bCarried )
		{
			SendWeaponAnim( ACT_SLAM_TRIPMINE_IDLE );
		}
		else
		{
			SendWeaponAnim( ACT_SLAM_DETONATOR_IDLE );
		}

		SetWeaponIdleTime( gpGlobals->curtime + 1.0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: My mortar object is reloading
//-----------------------------------------------------------------------------
void CWeaponMortar::MortarIsReloading( void )
{
	m_bMortarReloading = true;
}

//-----------------------------------------------------------------------------
// Purpose: My mortar object has finished reloading
//-----------------------------------------------------------------------------
void CWeaponMortar::MortarFinishedReloading( void )
{
	m_bMortarReloading = false;
}

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
