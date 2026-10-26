//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose:		This is the base version of the combine (not instanced only subclassed)
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#ifndef	NPC_CONSCRIPT_H
#define	NPC_CONSCRIPT_H
#pragma once

#include "ai_playerally.h"
#include "ai_behavior_follow.h"

//=========================================================
//	>> CNPC_Conscript
//=========================================================
class CNPC_Conscript : public CAI_PlayerAlly
{
	DECLARE_CLASS( CNPC_Conscript, CAI_PlayerAlly );
public:
	CNPC_Conscript();
	bool			CreateBehaviors();
	void			StartTask( const Task_t *pTask );
	void			FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void			StopFollowing();
	bool			IsValidSpeechTarget( int flags, CBaseEntity *pEntity );
	bool			IsPlayerAlly( CBasePlayer *pPlayer = NULL );
	void			TrySmellTalk();
	void			Event_Killed( const CTakeDamageInfo &info );
	void			SetGunDrawn( bool drawn );
	void			Spawn( void );
	void			Precache( void );
	float			MaxYawSpeed( void );
	int				GetSoundInterests( void );
	void			ConscriptFirePistol( void );
	void			AlertSound( void );
	Class_T			Classify ( void );
	void			HandleAnimEvent( animevent_t *pEvent );
	bool			HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt);

	void			RunTask( const Task_t *pTask );
	int				ObjectCaps( void ) { return UsableNPCObjectCaps( BaseClass::ObjectCaps() ); }
	int				OnTakeDamage_Alive( const CTakeDamageInfo &info );
	Vector			BodyTarget( const Vector &posSrc, bool bNoisy = true );

	Activity		GetFollowActivity( float flDistance ) { return ACT_RUN; }

	void			DeclineFollowing( void );

	Activity		NPC_TranslateActivity( Activity eNewActivity );

	// Override these to set behavior
	virtual int		TranslateSchedule( int scheduleType );
	virtual int		SelectSchedule( void );

	void			DeathSound( const CTakeDamageInfo &info );
	void			PainSound( const CTakeDamageInfo &info );
	
	void			TalkInit( void );

	void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );

	bool			m_fGunDrawn;
	float			m_painTime;
	float			m_checkAttackTime;
	float			m_nextLineFireTime;
	bool			m_lastAttackCheck;
	bool			m_bInBarnacleMouth;
	CAI_FollowBehavior m_FollowBehavior;


	//=========================================================
	// Conscript Tasks
	//=========================================================
	enum 
	{
		TASK_CONSCRIPT_CROUCH = BaseClass::NEXT_TASK,
		TASK_CONSCRIPT_HEADRESET,
	};

	//=========================================================
	// Conscript schedules
	//=========================================================
	enum
	{
		SCHED_CONSCRIPT_FOLLOW = BaseClass::NEXT_SCHEDULE,
		SCHED_CONSCRIPT_DRAW,
		SCHED_CONSCRIPT_FACE_TARGET,
		SCHED_CONSCRIPT_STAND,
		SCHED_CONSCRIPT_AIM,
		SCHED_CONSCRIPT_BARNACLE_HIT,
		SCHED_CONSCRIPT_BARNACLE_PULL,
		SCHED_CONSCRIPT_BARNACLE_CHOMP,
		SCHED_CONSCRIPT_BARNACLE_CHEW,
	};


public:
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};

#endif	//NPC_CONSCRIPT_H
