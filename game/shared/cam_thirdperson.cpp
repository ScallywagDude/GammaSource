//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "cam_thirdperson.h"
#include "gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static Vector CAM_HULL_MIN(-CAM_HULL_OFFSET,-CAM_HULL_OFFSET,-CAM_HULL_OFFSET);
static Vector CAM_HULL_MAX( CAM_HULL_OFFSET, CAM_HULL_OFFSET, CAM_HULL_OFFSET);

#ifdef CLIENT_DLL

#include "input.h"


extern const ConVar *sv_cheats;

extern ConVar cam_idealdist;
extern ConVar cam_idealdistright;
extern ConVar cam_idealdistup;
extern ConVar cam_collision;

void CAM_ToThirdPerson(void);
void CAM_ToFirstPerson(void);

void ToggleThirdPerson( bool bValue )
{
	if ( bValue == true )
	{
		CAM_ToThirdPerson();
	}
	else
	{
		CAM_ToFirstPerson();
	}
}

void ThirdPersonChange( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );

	ToggleThirdPerson( var.GetBool() );
}

#if defined( HL2_CLIENT_DLL ) && defined( GAMMA_PORTALGUN )
ConVar cl_thirdperson( "cl_thirdperson", "0", FCVAR_USERINFO | FCVAR_ARCHIVE, "Enables/Disables third person", ThirdPersonChange );
#else
ConVar cl_thirdperson( "cl_thirdperson", "0", FCVAR_NOT_CONNECTED | FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_DEVELOPMENTONLY, "Enables/Disables third person", ThirdPersonChange );
#endif

#endif

void CThirdPersonManager::Init( void )
{
	m_bOverrideThirdPerson = false;
	m_bForced = false;
	m_bUseCameraOffsets = false;
	m_vecCameraOffset.Init();
	m_vecDesiredCameraOffset.Init();
	m_vecCameraOrigin.Init();
	m_ViewAngles.Init();
	ResetCameraCollision();

	m_flUpOffset = CAMERA_UP_OFFSET;

	if ( input )
	{
		input->CAM_SetCameraThirdData( NULL, vec3_angle );
	}
}

static bool IsThirdPersonCameraAllowed()
{
	if ( !GameRules() ) return false;
#if defined( HL2_CLIENT_DLL ) && defined( GAMMA_PORTALGUN )
	// Selectable characters are a normal single-player presentation setting.
	if ( gpGlobals->maxClients == 1 ) return true;
#endif
	return GameRules()->AllowThirdPersonCamera();
}

void CThirdPersonManager::ResetCameraCollision()
{
	// All four values are read before the first collision trace on mode changes.
	m_flFraction = m_flTargetFraction = 1.0f;
	m_flUpFraction = m_flTargetUpFraction = 1.0f;
	m_flLerpTime = m_flUpLerpTime = 0.0f;
}

void CThirdPersonManager::Update( void )
{

#ifdef CLIENT_DLL
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}

	// If cheats have been disabled, pull us back out of third-person view.
	if ( sv_cheats && !sv_cheats->GetBool() && GameRules() && !IsThirdPersonCameraAllowed() )
	{
		if ( (bool)input->CAM_IsThirdPerson() == true )
		{
			input->CAM_ToFirstPerson();
		}
		return;
	}

	if ( IsOverridingThirdPerson() == false )
	{
		if ( (bool)input->CAM_IsThirdPerson() != ( cl_thirdperson.GetBool() || m_bForced ) && IsThirdPersonCameraAllowed() )
		{
			ToggleThirdPerson( m_bForced || cl_thirdperson.GetBool() );
		}
	}
#endif

}

Vector CThirdPersonManager::GetDesiredCameraOffset( void )
{ 
	if ( IsOverridingThirdPerson() == true )
	{
		return Vector( cam_idealdist.GetFloat(), cam_idealdistright.GetFloat(), cam_idealdistup.GetFloat() );
	}

	return m_vecDesiredCameraOffset; 
}

Vector CThirdPersonManager::GetFinalCameraOffset( void )
{
	Vector vDesired = GetDesiredCameraOffset();
	if ( IsOverridingThirdPerson() )
	{
		// Use the distance CAM_Think actually interpolated, rather than jumping to
		// the ideal distance while collision is testing a different camera arm.
		vDesired[DIST_FORWARD] = m_vecCameraOffset[DIST];
		return vDesired;
	}

	if ( m_flUpFraction != 1.0f )
	{
		vDesired.z += m_flUpOffset;
	}

	return vDesired;

}

Vector CThirdPersonManager::GetDistanceFraction( void )
{
	float flFraction = m_flFraction;
	float flUpFraction = m_flUpFraction;

	float flFrac = RemapValClamped( gpGlobals->curtime - m_flLerpTime, 0, CAMERA_OFFSET_LERP_TIME, 0, 1 );

	flFraction = Lerp( flFrac, m_flFraction, m_flTargetFraction );

	if ( flFrac == 1.0f )
	{
		m_flFraction = m_flTargetFraction;
	}
	if ( IsOverridingThirdPerson() )
		return Vector( flFraction, flFraction, flFraction );

	flFrac = RemapValClamped( gpGlobals->curtime - m_flUpLerpTime, 0, CAMERA_UP_OFFSET_LERP_TIME, 0, 1 );

	flUpFraction = 1.0f - Lerp( flFrac, m_flUpFraction, m_flTargetUpFraction );

	if ( flFrac == 1.0f )
	{
		m_flUpFraction = m_flTargetUpFraction;
	}

	return Vector( flFraction, flFraction, flUpFraction );
}

void CThirdPersonManager::PositionCamera( CBasePlayer *pPlayer, QAngle angles, const Vector *pViewOrigin )
{
	if ( !pPlayer || !cam_collision.GetBool() )
	{
		ResetCameraCollision();
		return;
	}
	if ( pPlayer )
	{
		trace_t trace;

		Vector camForward, camRight, camUp;

		// CalcView may transform the eye through a portal or a moving parent.
		// Use that final world-space origin when the renderer supplies it.
		Vector origin = pViewOrigin ? *pViewOrigin : pPlayer->EyePosition();

		AngleVectors( angles, &camForward, &camRight, &camUp );
	
		Vector endPos = origin;

		Vector cameraOffset = GetFinalCameraOffset();
		Vector vecCamOffset = endPos - camForward * cameraOffset[DIST_FORWARD] + camRight * cameraOffset[DIST_RIGHT] + camUp * cameraOffset[DIST_UP];

		// use our previously #defined hull to collision trace
		CTraceFilterSimple traceFilter( pPlayer, COLLISION_GROUP_NONE );
		UTIL_TraceHull( endPos, vecCamOffset, CAM_HULL_MIN, CAM_HULL_MAX, MASK_SOLID & ~CONTENTS_MONSTER, &traceFilter, &trace );
		
		float targetFraction = trace.startsolid ? 0.0f : clamp( trace.fraction, 0.0f, 1.0f );
		if ( targetFraction != m_flTargetFraction )
		{
			m_flFraction = GetDistanceFraction().x;
			m_flLerpTime = gpGlobals->curtime;
		}

		m_flTargetFraction = targetFraction;
		m_flTargetUpFraction = 1.0f;

		//If we're getting closer to a wall snap the fraction right away.
		if ( m_flTargetFraction < m_flFraction )
		{
			m_flFraction = m_flTargetFraction;
			m_flLerpTime = gpGlobals->curtime;
		}
	

		// move the camera closer if it hit something
		if( trace.fraction < 1.0  )
		{
			UTIL_TraceHull( endPos, endPos - camForward * cameraOffset[DIST_FORWARD], CAM_HULL_MIN, CAM_HULL_MAX, MASK_SOLID & ~CONTENTS_MONSTER, &traceFilter, &trace );

			if ( trace.fraction != 1.0f )
			{
				if ( trace.fraction != m_flTargetUpFraction )
				{
					m_flUpLerpTime = gpGlobals->curtime;
				}

				m_flTargetUpFraction = trace.fraction;

				if ( m_flTargetUpFraction < m_flUpFraction )
				{
					m_flUpFraction = trace.fraction;
					m_flUpLerpTime = gpGlobals->curtime;
				}
			}
		}
	}
}

bool CThirdPersonManager::WantToUseGameThirdPerson( void )
{
	return cl_thirdperson.GetBool() && IsThirdPersonCameraAllowed() && IsOverridingThirdPerson() == false;
}

bool CThirdPersonManager::IsGameplayThirdPerson( void ) const
{
#if defined( CLIENT_DLL ) && defined( HL2_CLIENT_DLL ) && defined( GAMMA_PORTALGUN )
	return cl_thirdperson.GetBool();
#else
	return false;
#endif
}


CThirdPersonManager g_ThirdPersonManager;
