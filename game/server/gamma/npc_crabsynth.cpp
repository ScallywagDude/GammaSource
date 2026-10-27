#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_hull.h"
#include "ai_route.h"
#include "AI_Navigator.h"
#include "AI_Memory.h"
#include "crabsynth_charge_policy.h"
#include "IEffects.h"
#include "npcevent.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

extern ISoundEmitterSystemBase *soundemitterbase;

static bool HasScriptSound( const char *pszName )
{
	return soundemitterbase->IsValidIndex( soundemitterbase->GetSoundIndex( pszName ) );
}

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CCrabSynth : public CAI_BaseNPC
{
	DECLARE_CLASS( CCrabSynth, CAI_BaseNPC );
	DECLARE_DATADESC();

public:
	void Spawn( void );
	void Precache( void );

	Class_T Classify( void ) { return CLASS_MORTAR_SYNTH; }
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual int	SelectSchedule( void );
	virtual int TranslateSchedule( int scheduleType );
	virtual void StartTask( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );
	virtual float MaxYawSpeed( void );

	// GammaSource: the 2003 Crab Synth left melee and its voice unwritten
	// (see the #if 0 below); these finish it.
	virtual int MeleeAttack1Conditions( float flDot, float flDist );
	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual void IdleSound( void );
	virtual void AlertSound( void );
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void DeathSound( const CTakeDamageInfo &info );

#if 0
	void HandleAnimEvent( animevent_t *pEvent );

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );

#endif

protected:
	DEFINE_CUSTOM_AI;
#if 0
	DECLARE_DATADESC();

#endif

	float		m_nextCharge;

private:
	void		MeleeHit( void );
	void		Voice( const char *pszCrabSound, const char *pszFallback );

	bool		m_bMeleeHit;
	float		m_flNextPainSound;
};


ConVar sk_crabsynth_charge_dmg( "sk_crabsynth_charge_dmg", "35", FCVAR_NONE,
    "Damage from a Crab Synth charge impact (0 disables damage)." );
ConVar sk_crabsynth_health( "sk_crabsynth_health", "251", FCVAR_NONE,
    "Crab Synth health (251 in the 2003 code)." );
ConVar sk_crabsynth_melee_dmg( "sk_crabsynth_melee_dmg", "25", FCVAR_NONE,
    "Damage from a Crab Synth claw swipe." );

LINK_ENTITY_TO_CLASS( npc_crabsynth, CCrabSynth );
IMPLEMENT_CUSTOM_AI( npc_crabsynth, CCrabSynth );


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CCrabSynth )

	DEFINE_FIELD( m_nextCharge,	FIELD_TIME ),
	DEFINE_FIELD( m_bMeleeHit, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flNextPainSound, FIELD_TIME ),

END_DATADESC()


//=========================================================
// private activities
//=========================================================
int ACT_CHARGE_START;
int ACT_CHARGE_START_MOVE;
int ACT_CHARGE_LOOP;
int ACT_CHARGE_END;
int ACT_BODY_THROW;
int ACT_CHARGE_HITLEFT;
int ACT_CHARGE_HITRIGHT;

//=========================================================
// schedules
//=========================================================
enum
{
	SCHED_CRAB_CHARGE = LAST_SHARED_SCHEDULE,
	SCHED_CRAB_CHARGE_STOP,
	SCHED_CRAB_FLINCH,
	SCHED_CRAB_CHARGE_HITLEFT,
	SCHED_CRAB_CHARGE_HITRIGHT,
};

//=========================================================
// tasks
//=========================================================
enum 
{
	TASK_CHARGE = LAST_SHARED_TASK,
	TASK_GET_CHARGE_PATH,
	TASK_CHARGE_HIT,
	TASK_CHARGE_HITLEFT,
	TASK_CHARGE_HITRIGHT,
};

//=========================================================
//=========================================================
#if 0
BEGIN_DATADESC( CCrabSynth )


END_DATADESC()
#endif


void CCrabSynth::Precache( void )
{
	engine->PrecacheModel( "models/synth.mdl" );
	static const char *const s_ppszVoice[] =
	{
		"NPC_CrabSynth.Charge", "NPC_CrabSynth.Idle", "NPC_CrabSynth.Alert", "NPC_CrabSynth.Pain", "NPC_CrabSynth.Die",
		"NPC_Strider.Alert", "NPC_Strider.Hunt", "NPC_Strider.Pain", "NPC_Strider.Death",
	};
	for ( int i = 0; i < ARRAYSIZE( s_ppszVoice ); ++i )
	{
		if ( HasScriptSound( s_ppszVoice[i] ) )
			PrecacheScriptSound( s_ppszVoice[i] );
	}
	BaseClass::Precache();
}

void CCrabSynth::Spawn( void )
{
	// HACKHACK (2003): the crab is much bigger than its hull
	static const Vector s_CrabMins( -64, -64, 0 ), s_CrabMaxs( 64, 64, 80 );
	CollisionProp()->SetSurroundingBoundsType( USE_SPECIFIED_BOUNDS, &s_CrabMins, &s_CrabMaxs );
	Precache();
	SetModel( "models/synth.mdl" );
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();
	SetDefaultEyeOffset();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_GREEN );
	m_iHealth			= MAX( 1, sk_crabsynth_health.GetInt() );
	m_flFieldOfView		= 0.5f;
	m_NPCState			= NPC_STATE_NONE;
	
	m_nextCharge = 0.f;
	m_bMeleeHit = false;
	m_flNextPainSound = 0.f;

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 );

	NPCInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrabSynth::InitCustomSchedules( void )
{
	INIT_CUSTOM_AI( CCrabSynth );

	ADD_CUSTOM_TASK(CCrabSynth,			TASK_CHARGE);
	ADD_CUSTOM_TASK(CCrabSynth,			TASK_GET_CHARGE_PATH);
	ADD_CUSTOM_TASK(CCrabSynth,			TASK_CHARGE_HIT);
	ADD_CUSTOM_TASK(CCrabSynth,			TASK_CHARGE_HITLEFT);
	ADD_CUSTOM_TASK(CCrabSynth,			TASK_CHARGE_HITRIGHT);
	
	ADD_CUSTOM_SCHEDULE(CCrabSynth,		SCHED_CRAB_CHARGE);
	ADD_CUSTOM_SCHEDULE(CCrabSynth,		SCHED_CRAB_CHARGE_STOP);
	ADD_CUSTOM_SCHEDULE(CCrabSynth,		SCHED_CRAB_FLINCH);
	ADD_CUSTOM_SCHEDULE(CCrabSynth,		SCHED_CRAB_CHARGE_HITLEFT);
	ADD_CUSTOM_SCHEDULE(CCrabSynth,		SCHED_CRAB_CHARGE_HITRIGHT);

//	ADD_CUSTOM_CONDITION(CCrabSynth,	COND_);
	
	ADD_CUSTOM_ACTIVITY(CCrabSynth,		ACT_CHARGE_START);
	ADD_CUSTOM_ACTIVITY(CCrabSynth,		ACT_CHARGE_START_MOVE);
	ADD_CUSTOM_ACTIVITY(CCrabSynth,		ACT_CHARGE_LOOP);
	ADD_CUSTOM_ACTIVITY(CCrabSynth,		ACT_CHARGE_END);
	ADD_CUSTOM_ACTIVITY(CCrabSynth,		ACT_BODY_THROW);
	ADD_CUSTOM_ACTIVITY(CCrabSynth,		ACT_CHARGE_HITLEFT);
	ADD_CUSTOM_ACTIVITY(CCrabSynth,		ACT_CHARGE_HITRIGHT);

	AI_LOAD_SCHEDULE(CCrabSynth,		SCHED_CRAB_CHARGE);
	AI_LOAD_SCHEDULE(CCrabSynth,		SCHED_CRAB_CHARGE_STOP);
	AI_LOAD_SCHEDULE(CCrabSynth,		SCHED_CRAB_FLINCH);
	AI_LOAD_SCHEDULE(CCrabSynth,		SCHED_CRAB_CHARGE_HITLEFT);
	AI_LOAD_SCHEDULE(CCrabSynth,		SCHED_CRAB_CHARGE_HITRIGHT);
}

void CCrabSynth::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	CTakeDamageInfo info = inputInfo;

	if (info.GetDamageType() & (DMG_BULLET | DMG_SLASH | DMG_CLUB))
	{
		g_pEffects->Ricochet( ptr->endpos, (vecDir*-1.0f) );
		info.SetDamage( 0.01 );
	}

	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );
}

int CCrabSynth::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;
	info.SetDamage( 0.01f );
	if ( info.GetDamageType() & DMG_BLAST )
	{
		if ( info.GetInflictor() )
		{
			// damage from roughly below?
			if ( info.GetInflictor()->GetAbsOrigin().z - GetAbsOrigin().z < ((( GetAbsOrigin() + CollisionProp()->OBBMaxs() ).z-( GetAbsOrigin() + CollisionProp()->OBBMins() ).z)*0.5f) )
			{
				info.SetDamage( inputInfo.GetDamage() );
			}
		}
	}
	return BaseClass::OnTakeDamage_Alive( info );
}

int CCrabSynth::SelectSchedule( void )
{
	switch ( m_NPCState )
	{
	case NPC_STATE_COMBAT:
		if( HasCondition( COND_HEAVY_DAMAGE ) )
		{
			return SCHED_CRAB_FLINCH;
		}
		if ( HasCondition( COND_SEE_ENEMY ) )
		{
			if ( m_nextCharge < gpGlobals->curtime )
			{
				m_nextCharge = gpGlobals->curtime + 20.f;
				Voice( "NPC_CrabSynth.Charge", "NPC_Strider.Alert" );
				return SCHED_CRAB_CHARGE;
			}
		}
		break;

	default:
		return BaseClass::SelectSchedule();
		break;
	}

	return BaseClass::SelectSchedule();
}

int CCrabSynth::TranslateSchedule( int scheduleType )
{
	return BaseClass::TranslateSchedule( scheduleType );
}


float CCrabSynth::MaxYawSpeed( void )
{
	if ( GetActivity() == ACT_CHARGE_LOOP )
	{
		return 20.f;
	}
	return 90.f;
}

void CCrabSynth::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_CHARGE:
		break;
	case TASK_GET_CHARGE_PATH:
		{
			if (!GetEnemy() || IsUnreachable(GetEnemy()))
			{
				TaskFail(FAIL_NO_ROUTE);
				return;
			}
			Vector flEnemyLKP = GetEnemyLKP();
			if ( !GetNavigator()->SetGoal( flEnemyLKP, AIN_CLEAR_TARGET ) )
			{
				// no way to get there =(
				DevWarning( 2, "GetPathToEnemyLKP failed!!\n" );
				RememberUnreachable(GetEnemy());
				TaskFail(FAIL_NO_ROUTE);
				return;
			}
			GetNavigator()->SetMovementActivity( (Activity)ACT_CHARGE_LOOP );
			TaskComplete();
			break;
		}
		break;
	
	case TASK_CHARGE_HIT:
		GetNavigator()->SetMovementActivity( (Activity)ACT_CHARGE_END );
		SetIdealActivity( (Activity)ACT_CHARGE_END );
		break;
	case TASK_CHARGE_HITLEFT:
		GetNavigator()->SetMovementActivity( (Activity)ACT_CHARGE_HITLEFT );
		SetIdealActivity( (Activity)ACT_CHARGE_HITLEFT );
		break;
	case TASK_CHARGE_HITRIGHT:
		GetNavigator()->SetMovementActivity( (Activity)ACT_CHARGE_HITRIGHT );
		SetIdealActivity( (Activity)ACT_CHARGE_HITRIGHT );
		break;

	case TASK_MELEE_ATTACK1:
		m_bMeleeHit = false;
		BaseClass::StartTask( pTask );
		break;

	default:
		BaseClass::StartTask( pTask );
	}
}

#define CHARGE_ATTACK_HIT		120.f
#define CHARGE_ATTACK_WIDTH		100.f
#define CHARGE_ATTACK_TRAMPLE	40.f

void CCrabSynth::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_CHARGE:
		{
			CBaseEntity *pEnemy = GetEnemy();
			if ( GetNavigator()->GetGoalType() == GOALTYPE_NONE || !GetNavigator()->IsGoalActive() || !pEnemy )
			{
				TaskComplete();
				GetNavigator()->ClearGoal();		// Stop moving
				SetSchedule( SCHED_CRAB_CHARGE_STOP );
			}
			else
			{
				// Check validity of goal type
				ValidateNavGoal();
				SetIdealActivity( GetNavigator()->GetMovementActivity() );

				Vector forward, right;
				AngleVectors( GetLocalAngles(), &forward, &right, NULL );
                // HL2MODERN_CRABSYNTH_CHARGE_DAMAGE
                // Evaluate the forward/lateral projections with unit-tested
                // geometry before applying any server-side damage.
                const Vector delta = pEnemy->WorldSpaceCenter() - WorldSpaceCenter();
                const float forwardDistance = DotProduct( forward, delta );
                const float lateralDistance = DotProduct( right, delta );
                const HL2ModernCrabCharge::Decision decision =
                    HL2ModernCrabCharge::Classify(forwardDistance, lateralDistance,
                        CHARGE_ATTACK_HIT, CHARGE_ATTACK_WIDTH, CHARGE_ATTACK_TRAMPLE);
                if (decision != HL2ModernCrabCharge::APPROACH)
                {
                    // Do not hit a dead or occluded enemy. End a missed charge
                    // rather than repeatedly dealing damage during RunTask.
                    if (decision == HL2ModernCrabCharge::MISS ||
                        !pEnemy->IsAlive() || !FVisible(pEnemy))
                    {
                        SetSchedule(SCHED_CRAB_CHARGE_STOP);
                    }
                    else
                    {
                        const float damage = sk_crabsynth_charge_dmg.GetFloat();
                        if (damage > 0.0f && damage <= 1000.0f)
                        {
                            CTakeDamageInfo info(this, this, damage, DMG_CRUSH);
                            CalculateMeleeDamageForce(&info, forward, pEnemy->WorldSpaceCenter());
                            pEnemy->TakeDamage(info);
                        }

                        if (decision == HL2ModernCrabCharge::HIT_LEFT)
                            SetSchedule(SCHED_CRAB_CHARGE_HITLEFT);
                        else if (decision == HL2ModernCrabCharge::HIT_RIGHT)
                            SetSchedule(SCHED_CRAB_CHARGE_HITRIGHT);
                        else
                            TaskComplete();  // Continue to TASK_CHARGE_HIT.
                    }
                }
			}
		}
		break;
	case TASK_CHARGE_HITRIGHT:
	case TASK_CHARGE_HITLEFT:
	case TASK_CHARGE_HIT:
		if ( IsActivityFinished() )
		{
			GetNavigator()->ClearGoal();
			TaskComplete();
		}
		break;

	case TASK_MELEE_ATTACK1:
		// The swipe's model events are undocumented; strike mid-swing if no
		// event has done it yet.
		if ( !m_bMeleeHit && GetCycle() >= 0.4f )
		{
			MeleeHit();
		}
		BaseClass::RunTask( pTask );
		break;

	default:
		BaseClass::RunTask( pTask );
	}
}



//-----------------------------------------------------------------------------
// GammaSource: melee and voice, which the 2003 Crab Synth never got.
//-----------------------------------------------------------------------------
#define CRAB_MELEE_REACH	112.f

int CCrabSynth::MeleeAttack1Conditions( float flDot, float flDist )
{
	if ( !HaveSequenceForActivity( ACT_MELEE_ATTACK1 ) )
		return COND_NONE;
	if ( flDist > CRAB_MELEE_REACH )
		return COND_TOO_FAR_TO_ATTACK;
	if ( flDot < 0.7f )
		return COND_NOT_FACING_ATTACK;
	return COND_CAN_MELEE_ATTACK1;
}

void CCrabSynth::HandleAnimEvent( animevent_t *pEvent )
{
	// Any old-style event in the swipe is its strike.
	if ( !( pEvent->type & AE_TYPE_NEWEVENTSYSTEM ) && pEvent->event >= 1 && pEvent->event <= 9 &&
		GetActivity() == ACT_MELEE_ATTACK1 )
	{
		if ( !m_bMeleeHit )
		{
			MeleeHit();
		}
		return;
	}
	BaseClass::HandleAnimEvent( pEvent );
}

void CCrabSynth::MeleeHit( void )
{
	m_bMeleeHit = true;
	const float damage = sk_crabsynth_melee_dmg.GetFloat();
	CBaseEntity *pHurt = CheckTraceHullAttack( CRAB_MELEE_REACH, Vector( -24, -24, -24 ), Vector( 24, 24, 24 ),
		MAX( 0.f, damage ), DMG_SLASH, 1.0f, true );
	if ( pHurt && pHurt->IsPlayer() )
	{
		pHurt->ViewPunch( QAngle( random->RandomFloat( -8, -4 ), random->RandomFloat( -12, 12 ), 0 ) );
	}
	UTIL_ScreenShake( GetAbsOrigin(), 4.0f, 60.0f, 0.4f, 300.0f, SHAKE_START );
}

// Plays the Crab Synth's own sound if a sound script defines it, otherwise the
// Strider's (also a synth) pitched up for the smaller body.
void CCrabSynth::Voice( const char *pszCrabSound, const char *pszFallback )
{
	if ( HasScriptSound( pszCrabSound ) )
	{
		EmitSound( pszCrabSound );
		return;
	}

	CPASAttenuationFilter filter( this, pszFallback );
	EmitSound_t ep;
	ep.m_pSoundName = pszFallback;
	ep.m_nPitch = random->RandomInt( 125, 140 );
	ep.m_nFlags = SND_CHANGE_PITCH;
	EmitSound( filter, entindex(), ep );
}

void CCrabSynth::IdleSound( void )
{
	Voice( "NPC_CrabSynth.Idle", "NPC_Strider.Hunt" );
}

void CCrabSynth::AlertSound( void )
{
	Voice( "NPC_CrabSynth.Alert", "NPC_Strider.Alert" );
}

void CCrabSynth::PainSound( const CTakeDamageInfo &info )
{
	// Most hits are shrugged off by the shell; only voice real damage.
	if ( info.GetDamage() < 1.0f || gpGlobals->curtime < m_flNextPainSound )
		return;
	m_flNextPainSound = gpGlobals->curtime + 1.5f;
	Voice( "NPC_CrabSynth.Pain", "NPC_Strider.Pain" );
}

void CCrabSynth::DeathSound( const CTakeDamageInfo &info )
{
	Voice( "NPC_CrabSynth.Die", "NPC_Strider.Death" );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

//=========================================================
// Charge!
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_CRAB_CHARGE,

	"	Tasks"
	"		TASK_FACE_ENEMY					0"
	"		TASK_GET_CHARGE_PATH			0"
	"		TASK_CHARGE						0"
	"		TASK_CHARGE_HIT					0"
	"	"
	"	Interrupts"
	"		COND_HEAVY_DAMAGE"
);

//=========================================================
// Charge Hit Left!
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_CRAB_CHARGE_HITLEFT,

	"	Tasks"
	"		TASK_CHARGE_HITLEFT				0"
	"	"
	"	Interrupts"
);

//=========================================================
// Charge Hit Right!
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_CRAB_CHARGE_HITRIGHT,

	"	Tasks"
	"		TASK_CHARGE_HITRIGHT			0"
	"	"
	"	Interrupts"
);

//=========================================================
// Charge Stop
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_CRAB_CHARGE_STOP,

	"	Tasks"
	"		TASK_PLAY_PRIVATE_SEQUENCE		ACTIVITY:ACT_CHARGE_END"
	"	"
	"	Interrupts"
);

//=========================================================
// Flop / Flinch
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_CRAB_FLINCH,

	"	Tasks"
	"		TASK_PLAY_PRIVATE_SEQUENCE		ACTIVITY:ACT_BODY_THROW"
	"	"
	"	Interrupts"
);