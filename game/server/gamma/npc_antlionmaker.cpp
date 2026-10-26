//========= GammaSource: ported from the 2003 code (dlls/hl2_dll/npc_antlion.cpp, CAntlionMaker) ============//
//
// Purpose: npc_antlionmaker, the 2003 antlion spawner the bugbait and seafloor test maps use.
//			(The 2018 code only has npc_antlion_template_maker.)
//
//=============================================================================

#include "cbase.h"
#include "monstermaker.h"
#include "npc_antlion.h"
#include "ai_hint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SF_ANTLIONMAKER_SPAWN_FACING_PLAYER	0x00000200

class CAntlionMaker : public CNPCMaker
{
public:
	DECLARE_CLASS( CAntlionMaker, CNPCMaker );
	DECLARE_DATADESC();

	CAntlionMaker( void )
	{
		m_iszNPCClassname = MAKE_STRING( "npc_antlion" );
	}

	virtual void ChildPreSpawn( CAI_BaseNPC *pChild )
	{
		CNPC_Antlion *pAntlion = dynamic_cast<CNPC_Antlion *>( pChild );
		if ( pAntlion == NULL )
			return;

		pAntlion->m_bStartBurrowed = m_bStartBurrowed;

		// Spawn near the player if asked to.
		if ( m_strSpawnGroup != NULL_STRING )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
			if ( pPlayer )
			{
				CHintCriteria hintCriteria;
				hintCriteria.SetHintType( HINT_ANTLION_BURROW_POINT );
				hintCriteria.SetFlag( bits_HINT_NODE_NEAREST );
				hintCriteria.SetGroup( m_strSpawnGroup );
				// Spawn the dudes in the player's way.
				hintCriteria.AddIncludePosition( pPlayer->GetAbsOrigin(), 512 );

				CAI_Hint *pSearchHint = CAI_HintManager::FindHint( pChild, hintCriteria );
				if ( pSearchHint )
				{
					Vector vecOrigin;
					pSearchHint->GetPosition( pChild, &vecOrigin );
					UTIL_SetOrigin( pChild, vecOrigin );
				}
			}
		}
	}

	virtual void ChildPostSpawn( CAI_BaseNPC *pChild )
	{
		BaseClass::ChildPostSpawn( pChild );

		CNPC_Antlion *pAntlion = dynamic_cast<CNPC_Antlion *>( pChild );
		if ( pAntlion == NULL )
			return;

		if ( m_bStartBurrowed )
			pAntlion->BurrowUse( this, this, USE_ON, 0.0f );

		if ( HasSpawnFlags( SF_NPC_LONG_RANGE ) )
		{
			pAntlion->m_flDistTooFar = 1e9f;
			pAntlion->SetDistLook( 6000.0 );
		}

		// Face the child to the player if asked to.
		if ( HasSpawnFlags( SF_ANTLIONMAKER_SPAWN_FACING_PLAYER ) )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
			if ( pPlayer )
			{
				QAngle angles;
				Vector vecDir = pPlayer->GetAbsOrigin() - pChild->GetAbsOrigin();
				VectorAngles( vecDir, angles );
				angles.x = 0;
				angles.z = 0;
				pChild->SetLocalAngles( angles );
			}
		}
	}

	bool		m_bStartBurrowed;
	string_t	m_strSpawnGroup;	// if present, spawn children on the nearest node of this group (to the player)
};

LINK_ENTITY_TO_CLASS( npc_antlionmaker, CAntlionMaker );

BEGIN_DATADESC( CAntlionMaker )
	DEFINE_KEYFIELD( m_bStartBurrowed, FIELD_BOOLEAN, "startburrowed" ),
	DEFINE_KEYFIELD( m_strSpawnGroup, FIELD_STRING, "spawngroup" ),
END_DATADESC()
