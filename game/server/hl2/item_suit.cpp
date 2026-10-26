//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== item_suit.cpp ========================================================

  handling for the player's suit.
*/

#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "items.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SF_SUIT_SHORTLOGON		0x0001

// A private path prevents mounted retail content or an old converted cache from
// replacing the original 2003 suit pickup.
static const char *GAMMA_HEV_SUIT_MODEL = "models/items/gamma_hevsuit_2003.mdl";

class CItemSuit : public CItem
{
public:
	DECLARE_CLASS( CItemSuit, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( GAMMA_HEV_SUIT_MODEL );
		BaseClass::Spawn( );
		
		CollisionProp()->UseTriggerBounds( false, 0 );
	}
	void Precache( void )
	{
		PrecacheModel( GAMMA_HEV_SUIT_MODEL );
	}
	bool MyTouch( CBasePlayer *pPlayer )
	{
		if ( pPlayer->IsSuitEquipped() )
			return FALSE;

		if ( m_spawnflags & SF_SUIT_SHORTLOGON )
			UTIL_EmitSoundSuit(pPlayer->edict(), "!HEV_A0");		// short version of suit logon,
		else
			UTIL_EmitSoundSuit(pPlayer->edict(), "!HEV_AAx");	// long version of suit logon

		pPlayer->EquipSuit();
				
		return true;
	}
};

LINK_ENTITY_TO_CLASS(item_suit, CItemSuit);

CON_COMMAND_F( gamma_suitinfo, "Print every suit pickup model and position", FCVAR_CHEAT )
{
	int count = 0;
	for ( CBaseEntity *pSuit = gEntList.FindEntityByClassname( NULL, "item_suit" ); pSuit;
		pSuit = gEntList.FindEntityByClassname( pSuit, "item_suit" ) )
	{
		const Vector &origin = pSuit->GetAbsOrigin();
		Msg( "GAMMA_SUIT entity=%d model=%s origin=(%.1f %.1f %.1f)\n",
			pSuit->entindex(), STRING( pSuit->GetModelName() ), origin.x, origin.y, origin.z );
		++count;
	}
	Msg( "GAMMA_SUIT count=%d expected=%s\n", count, GAMMA_HEV_SUIT_MODEL );
}
