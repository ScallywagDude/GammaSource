//========= GammaSource: ported from the 2003 code (legacy2003/dlls/hl2_dll/npc_combine.h) ============//
//
// Purpose: 
//
//=============================================================================

#ifndef NPC_COMBINE2003_H
#define NPC_COMBINE2003_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"
#include "ai_basehumanoid.h"
#include "ai_behavior.h"
#include "ai_behavior_assault.h"
#include "ai_behavior_standoff.h"
#include "ai_behavior_follow.h"

//=========================================================
//	>> CNPC_Combine2003
//=========================================================
class CNPC_Combine2003 : public CAI_BaseHumanoid
{
	DECLARE_CLASS( CNPC_Combine2003, CAI_BaseHumanoid );

public:
	static const char *pCombineSentences[];

	int				m_nKickDamage;
	Vector			m_vecTossVelocity;
	bool			m_bStanding;
	bool			m_bCanCrouch;
	bool			m_bFirstEncounter;// only put on the handsign show in the squad's first encounter.
	int				m_iSentence;
	int				m_voicePitch;
	int				m_lastGrenadeCondition;

	void SetVoicePitch( int voicePitch )	{ m_voicePitch = voicePitch; }
	int GetVoicePitch() const				{ return m_voicePitch; }

	// ---------------
	// Time Variables
	// ---------------
	float			m_flNextPainSoundTime;
	float			m_flNextAlertSoundTime;
	float			m_flNextGrenadeCheck;	
	float			m_flNextLostSoundTime;
	float			m_flAlertPatrolTime;		// When to stop doing alert patrol

	CNPC_Combine2003();

	int				GetGrenadeConditions( float flDot, float flDist );
	int				RangeAttack2Conditions( float flDot, float flDist ); // For innate grenade attack
	int				MeleeAttack1Conditions( float flDot, float flDist ); // For kick/punch
	void			CheckAmmo( void );
	bool			FVisible( CBaseEntity *pEntity, int traceMask = MASK_OPAQUE, CBaseEntity **ppBlocker = NULL );

	NPC_STATE		SelectIdealState ( void );

	int				m_nShots;
	float			m_flShotDelay;

	// Input handlers.
	void InputLookOn( inputdata_t &inputdata );
	void InputLookOff( inputdata_t &inputdata );

	bool			UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer = NULL );

	void			Spawn( void );
	void			Precache( void );

	Class_T			Classify( void );
	float			MaxYawSpeed( void );
  	bool			OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval );;
	void			HandleAnimEvent( animevent_t *pEvent );
	Vector			Weapon_ShootPosition( );
	CBaseEntity		*Kick( void );

	Vector			EyeOffset( Activity nActivity );
	Vector			EyePosition( void );
	Vector			BodyTarget( const Vector &posSrc, bool bNoisy = true );

	void			StartTask( const Task_t *pTask );
	void			RunTask( const Task_t *pTask );

	Activity		NPC_TranslateActivity( Activity eNewActivity );
	Activity		CustomActivityOr( Activity custom, Activity fallback );
	void			BuildScheduleTestBits( void );
	virtual int		SelectSchedule( void );
	int				SelectScheduleAttack();

	bool			CreateBehaviors();

	bool			OnBeginMoveAndShoot();
	void			OnEndMoveAndShoot();

	// Combat
	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );


	// -------------
	// Sounds
	// -------------
	void			DeathSound( const CTakeDamageInfo &info );
	void			PainSound( const CTakeDamageInfo &info );
	void			IdleSound( void );
	void			AlertSound( void );
	void			LostEnemySound( void );
	void			FoundEnemySound( void );
	void			AnnounceAssault( void );

	void			NotifyDeadFriend( CBaseEntity* pFriend );

	virtual float	HearingSensitivity( void ) { return 1.0; };
	int				GetSoundInterests( void );

	// Speaking
	void			SpeakSentence( int sentType );

	virtual int		TranslateSchedule( int scheduleType );

	//=========================================================
	// Combine S schedules
	//=========================================================
	enum
	{
		SCHED_COMBINE2003_SUPPRESS = BaseClass::NEXT_SCHEDULE,
		SCHED_COMBINE2003_COMBAT_FAIL,
		SCHED_COMBINE2003_VICTORY_DANCE,
		SCHED_COMBINE2003_COMBAT_FACE,
		SCHED_COMBINE2003_HIDE_AND_RELOAD,
		SCHED_COMBINE2003_SIGNAL_SUPPRESS,
		SCHED_COMBINE2003_ASSAULT,
		SCHED_COMBINE2003_ESTABLISH_LINE_OF_FIRE,
		SCHED_COMBINE2003_PRESS_ATTACK,
		SCHED_COMBINE2003_REPEL,
		SCHED_COMBINE2003_REPEL_ATTACK,
		SCHED_COMBINE2003_REPEL_LAND,
		SCHED_COMBINE2003_WAIT_IN_COVER,
		SCHED_COMBINE2003_RANGE_ATTACK1,
		SCHED_COMBINE2003_RANGE_ATTACK2,
		SCHED_COMBINE2003_TAKE_COVER1,
		SCHED_COMBINE2003_TAKE_COVER_FROM_BEST_SOUND,
		SCHED_COMBINE2003_RUN_AWAY_FROM_BEST_SOUND,
		SCHED_COMBINE2003_GRENADE_COVER1,
		SCHED_COMBINE2003_TOSS_GRENADE_COVER1,
		SCHED_COMBINE2003_TAKECOVER_FAILED,
		SCHED_COMBINE2003_SHOOT_ENEMY_COVER,
		SCHED_COMBINE2003_GRENADE_AND_RELOAD,
		SCHED_COMBINE2003_PATROL,
		NEXT_SCHEDULE,
	};

	//=========================================================
	// Combine Tasks
	//=========================================================
	enum 
	{
		TASK_COMBINE2003_FACE_TOSS_DIR = BaseClass::NEXT_TASK,
		TASK_COMBINE2003_IGNORE_ATTACKS,
		//TASK_COMBINE2003_MOVE_AND_SHOOT,
		//TASK_COMBINE2003_MOVE_AND_AIM,
		NEXT_TASK
	};

	//=========================================================
	// Combine Conditions
	//=========================================================
	enum Combine_Conds
	{
		COND_COMBINE2003_NO_FIRE = BaseClass::NEXT_CONDITION,
		COND_COMBINE2003_DEAD_FRIEND,
		COND_COMBINE2003_SHOULD_PATROL,
		NEXT_CONDITION
	};

public:
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

private:

	class CCombine2003StandoffBehavior : public CAI_ComponentWithOuter<CNPC_Combine2003, CAI_StandoffBehavior>
	{
		typedef CAI_ComponentWithOuter<CNPC_Combine2003, CAI_StandoffBehavior> BaseClass;

		virtual int SelectScheduleAttack()
		{
			int result = GetOuter()->SelectScheduleAttack();
			if ( result == SCHED_NONE )
				result = BaseClass::SelectScheduleAttack();
			return result;
		}
	};

	int			m_iNumGrenades;
	CAI_AssaultBehavior			m_AssaultBehavior;
	CCombine2003StandoffBehavior	m_StandoffBehavior;
	CAI_FollowBehavior			m_FollowBehavior;
};


//=========================================================
//	>> CNPC_CombineS2003
//=========================================================
class CNPC_CombineS2003 : public CNPC_Combine2003
{
	DECLARE_CLASS( CNPC_CombineS2003, CNPC_Combine2003 );

public:
	void		Spawn( void );
	void		Precache( void );
	void		DeathSound( const CTakeDamageInfo &info );
	void		MaintainLookTargets( float flInterval );
	void		PrescheduleThink( void );
	void		BuildScheduleTestBits( void );
	int			SelectSchedule ( void );
	int			OnTakeDamage( const CTakeDamageInfo &info );	// 2003 TakeDamage override
	int			OnTakeDamage_Alive( const CTakeDamageInfo &info );
	bool		FInAimCone( const Vector &vecSpot );
	void		HandleAnimEvent( animevent_t *pEvent );
	void		OnChangeActivity( Activity eNewActivity );
	void		Event_Killed( const CTakeDamageInfo &info );
	void		OnListened();

	void		ClearAttackConditions( void );

	// 2003 GetWeaponProficiency override (HIGH); 2018 picks proficiency here.
	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon ) { return WEAPON_PROFICIENCY_VERY_GOOD; }

	bool		m_fIsBlocking;

private:
	bool		ShouldHitPlayer( const Vector &targetDir, float targetDist );
};

#endif // NPC_COMBINE2003_H
