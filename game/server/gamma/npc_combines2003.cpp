//========= GammaSource: ported from the 2003 code (legacy2003/dlls/hl2_dll/npc_combines.cpp) ============//
//
// Purpose: This is the soldier version of the combine, analogous to the HL1 grunt.
//
//=============================================================================

#include "cbase.h"
#include "ai_hull.h"
#include "ai_motor.h"
#include "npc_talker.h"
#include "npc_combine2003.h"
#include "npc_combines.h"		// the HL2 soldier, for the classname selector
#include "gamma_entity_select.h"
#include "bitstring.h"
#include "engine/IEngineSound.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "npcevent.h"
#include "hl2_player.h"
#include "game.h"
#include "ammodef.h"
#include "explode.h"
#include "ai_memory.h"
#include "Sprite.h"
#include "soundenvelope.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Shared with the HL2 soldier (hl2/npc_combines.cpp), which defines them.
extern ConVar	sk_combine_s_health;
extern ConVar	sk_combine_s_kick;

//Whether or not the combine should spawn health on death
extern ConVar	combine_spawn_health;

LINK_ENTITY_TO_CLASS( npc_combine_s_2003, CNPC_CombineS2003 );

// GammaSource: the 2003 soldier AI is a line-for-line port of the 2003 source.
// Maps place npc_combine / npc_combine_s; this picks which AI they get. The
// _2003 classnames always give the 2003 AI.
ConVar gamma_combine_ai2003( "gamma_combine_ai2003", "1", FCVAR_ARCHIVE,
	"Combine soldiers (npc_combine, npc_combine_s) use the 2003 AI (1) or the HL2 AI (0). Change between maps." );

GAMMA_LINK_ENTITY_TO_CLASS_SELECT( npc_combine, gamma_combine_ai2003, CNPC_Combine2003, CNPC_Combine );
GAMMA_LINK_ENTITY_TO_CLASS_SELECT( npc_combine_s, gamma_combine_ai2003, CNPC_CombineS2003, CNPC_CombineS );


#define AE_SOLDIER_BLOCK_PHYSICS		20 // trying to block an incoming physics object

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_CombineS2003::Spawn( void )
{
	Precache();
	// GammaSource: honour a map-set model (HL2 maps use variants); 2003 always
	// used Combine_Soldier.mdl.
	SetModel( STRING( GetModelName() ) );
	m_iHealth = sk_combine_s_health.GetFloat();
	m_nKickDamage = sk_combine_s_kick.GetFloat();

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_INNATE_RANGE_ATTACK2 );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CombineS2003::Precache()
{
	if ( !GetModelName() )
	{
		SetModelName( MAKE_STRING( "models/combine_soldier.mdl" ) );
	}
	PrecacheModel( STRING( GetModelName() ) );

	UTIL_PrecacheOther( "item_healthvial" );
	UTIL_PrecacheOther( "weapon_frag" );

	BaseClass::Precache();
}


void CNPC_CombineS2003::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_CombineS.Die" );
}


//-----------------------------------------------------------------------------
// Purpose: maintains blending states of head, eyes, body, etc. 
//			FIXME: move this to CNPC_Combine2003?
//			FIXME: this needs to be moved/renamed once normal blended animation control gets added
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CombineS2003::MaintainLookTargets ( float flInterval )
{
	// maintain anim vectors
	Vector vecShootOrigin;

	vecShootOrigin = Weapon_ShootPosition();
	Vector vecShootDir = GetShootEnemyDir( vecShootOrigin );

	SetAim( vecShootDir );

	BaseClass::MaintainLookTargets( flInterval );
}

//-----------------------------------------------------------------------------
// Purpose: Soldiers use CAN_RANGE_ATTACK2 to indicate whether they can throw
//			a grenade. Because they check only every half-second or so, this
//			condition must persist until it is updated again by the code
//			that determines whether a grenade can be thrown, so prevent the 
//			base class from clearing it out. (sjb)
//-----------------------------------------------------------------------------
void CNPC_CombineS2003::ClearAttackConditions( )
{
	bool fCanRangeAttack2 = HasCondition( COND_CAN_RANGE_ATTACK2 );

	// Call the base class.
	BaseClass::ClearAttackConditions();

	if( fCanRangeAttack2 )
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition( COND_CAN_RANGE_ATTACK2 );
	}
}

void CNPC_CombineS2003::PrescheduleThink( void )
{
	/*//FIXME: This doesn't need to be in here, it's all debug info
	if( HasCondition( COND_HEAR_PHYSICS_DANGER ) )
	{
		// Don't react unless we see the item!!
		CSound *pSound = NULL;

		pSound = GetLoudestSoundOfType( SOUND_PHYSICS_DANGER );

		if( pSound )
		{
			if( FInViewCone( pSound->GetSoundReactOrigin() ) )
			{
				Msg( "OH CRAP!\n" );
				NDebugOverlay::Line( EyePosition(), pSound->GetSoundReactOrigin(), 0, 0, 255, false, 2.0f );
			}
		}
	}
	*/

	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_CombineS2003::BuildScheduleTestBits( void )
{
	// GammaSource: chain to the base soldier and its behaviors, as the comment
	// above asks; the 2003 code skipped them.
	BaseClass::BuildScheduleTestBits();

	//Interrupt any schedule with physics danger (as long as I'm not moving or already trying to block)
	if ( m_flGroundSpeed == 0.0 && !IsCurSchedule( SCHED_FLINCH_PHYSICS ) )
	{
		SetCustomInterruptCondition( COND_HEAR_PHYSICS_DANGER );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CombineS2003::SelectSchedule ( void )
{
	if( HasCondition( COND_HEAR_PHYSICS_DANGER ) )
	{
		return SCHED_FLINCH_PHYSICS;
	}

	return BaseClass::SelectSchedule();
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CombineS2003::OnTakeDamage( const CTakeDamageInfo &info )
{
	if( info.GetInflictor() && info.GetInflictor()->VPhysicsGetObject() )
	{
		// Hit by a physics object! Was I blocking?
		if( m_fIsBlocking )
		{
			IPhysicsObject *pPhysObject;

			pPhysObject = info.GetInflictor()->VPhysicsGetObject();

			if( pPhysObject )
			{
				// Only deflect objects of relatively low mass
				//Msg( "MASS: %f\n", pPhysObject->GetMass() );

				if( pPhysObject->GetMass() <= 30.0 )
				{
					// No damage from light objects (tuned for melons)
					return 0;
				}
			}
		}
	}

	return BaseClass::OnTakeDamage( info );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int	CNPC_CombineS2003::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	RestartGesture( ACT_GESTURE_SMALL_FLINCH );
	return BaseClass::OnTakeDamage_Alive( info );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CNPC_CombineS2003::FInAimCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - GetAbsOrigin() );

	// do this in 2D
	los.z = 0;
	VectorNormalize( los );

	Vector facingDir = BodyDirection2D();

	float flDot = DotProduct( los, facingDir );

	if ( flDot > 0.866 ) //30 degrees
		return true;

	return false;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CombineS2003::HandleAnimEvent( animevent_t *pEvent )
{
	// Old-style model events only; see CNPC_Combine2003::HandleAnimEvent.
	switch( ( pEvent->type & AE_TYPE_NEWEVENTSYSTEM ) ? -1 : pEvent->event )
	{
	case AE_SOLDIER_BLOCK_PHYSICS:
		DevMsg( 2, "BLOCKING!\n" );
		m_fIsBlocking = true;
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

void CNPC_CombineS2003::OnChangeActivity( Activity eNewActivity )
{
	// Any new sequence stops us blocking.
	m_fIsBlocking = false;
	BaseClass::OnChangeActivity( eNewActivity );
}

void CNPC_CombineS2003::OnListened()
{
	BaseClass::OnListened();

	if ( HasCondition( COND_HEAR_DANGER ) && HasCondition( COND_HEAR_PHYSICS_DANGER ) )
	{
		if ( HasInterruptCondition( COND_HEAR_DANGER ) )
		{
			ClearCondition( COND_HEAR_PHYSICS_DANGER );
		}
	}

	// debugging to find missed schedules
#if 0
	if ( HasCondition( COND_HEAR_DANGER ) && !HasInterruptCondition( COND_HEAR_DANGER ) )
	{
		Msg("Ignore danger in %s\n", GetCurSchedule()->GetName() );
	}
#endif
}

#define MAX_SHOT_DISTANCE	2048

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CNPC_CombineS2003::Event_Killed( const CTakeDamageInfo &info )
{
	if ( combine_spawn_health.GetBool() == false )
	{
		BaseClass::Event_Killed( info );
		return;
	}

	CBasePlayer *pPlayer = ToBasePlayer( info.GetAttacker() );

	if ( pPlayer != NULL )
	{
		//Try to throw dynamic health
		float healthPerc = ( (float) pPlayer->m_iHealth / (float) pPlayer->m_iMaxHealth );

		if ( random->RandomFloat( 0.0f, 1.0f ) > healthPerc*1.5f )
		{
			CBaseEntity *pItem = DropItem( "item_healthvial", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );

			if ( pItem )
			{
				IPhysicsObject *pObj = pItem->VPhysicsGetObject();

				if ( pObj )
				{
					Vector			vel		= RandomVector( -64.0f, 64.0f );
					AngularImpulse	angImp	= RandomAngularImpulse( -300.0f, 300.0f );

					vel[2] = 0.0f;
					pObj->AddVelocity( &vel, &angImp );
				}
			}
		}

		//Try to throw dynamic grenades
		int grenadeIndex = GetAmmoDef()->Index( "grenade" );
		int numGrenades = pPlayer->GetAmmoCount( grenadeIndex );

		if ( ( numGrenades < GetAmmoDef()->MaxCarry( grenadeIndex ) ) && ( random->RandomInt( 0, 4 ) == 0 ) )
		{
			CBaseEntity *pItem = DropItem( "weapon_frag", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );

			if ( pItem )
			{
				IPhysicsObject *pObj = pItem->VPhysicsGetObject();

				if ( pObj )
				{
					Vector			vel		= RandomVector( -64.0f, 64.0f );
					AngularImpulse	angImp	= RandomAngularImpulse( -300.0f, 300.0f );

					vel[2] = 0.0f;
					pObj->AddVelocity( &vel, &angImp );
				}
			}
		}
	}

	BaseClass::Event_Killed( info );
}


// The 2003 bounce bomb (combine_mine / bounce_bomb) is not ported here: the HL2
// build already registers those classnames.
