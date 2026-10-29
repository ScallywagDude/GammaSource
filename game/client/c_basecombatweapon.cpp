//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side implementation of CBaseCombatWeapon.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "history_resource.h"
#include "iclientmode.h"
#include "iinput.h"
#include "weapon_selection.h"
#include "hud_crosshair.h"
#include "engine/ivmodelinfo.h"
#include "tier0/vprof.h"
#include "hltvcamera.h"
#include "tier1/KeyValues.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#ifdef GAMMA_PORTALGUN
#include "portal/PortalRender.h"
#include "portal/c_portal_player.h"
#include "gamma/gamma_weapon_worldmodel.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Gets the local client's active weapon, if any.
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *GetActiveWeapon( void )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( !player )
		return NULL;

	return player->GetActiveWeapon();
}

#ifdef GAMMA_PORTALGUN
static ConVar gamma_physgun_grip_x( "gamma_physgun_grip_x", "0", FCVAR_CHEAT, "Early gravity gun hand-local grip X" );
static ConVar gamma_physgun_grip_y( "gamma_physgun_grip_y", "0", FCVAR_CHEAT, "Early gravity gun hand-local grip Y" );
static ConVar gamma_physgun_grip_z( "gamma_physgun_grip_z", "0", FCVAR_CHEAT, "Early gravity gun hand-local grip Z" );

static void GammaWeaponGripStatus( const CCommand &args )
{
	C_BaseCombatWeapon *weapon = GetActiveWeapon();
	C_BaseCombatCharacter *owner = weapon ? weapon->GetOwner() : NULL;
	if ( !owner ) return;
	C_BaseAnimating::AutoAllowBoneAccess access( true, true );
	const int hand = owner->LookupBone( "ValveBiped.Bip01_R_Hand" );
	if ( hand < 0 || !owner->SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime ) ) return;
	const matrix3x4_t &matrix = owner->GetBone( hand );
	Msg( "GAMMA_GRIP model=%s offset=%.3f,%.3f,%.3f\n", weapon->GetWorldModel(),
		gamma_physgun_grip_x.GetFloat(), gamma_physgun_grip_y.GetFloat(), gamma_physgun_grip_z.GetFloat() );
	for ( int row = 0; row < 3; ++row )
		Msg( "GAMMA_GRIP hand[%d]=%.6f,%.6f,%.6f,%.6f\n", row,
			matrix[row][0], matrix[row][1], matrix[row][2], matrix[row][3] );
}
static ConCommand gamma_weapon_grip_status( "gamma_weapon_grip_status", GammaWeaponGripStatus, "Report native hand transform for weapon grip validation", FCVAR_CHEAT );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::SetDormant( bool bDormant )
{
	// If I'm going from active to dormant and I'm carried by another player, holster me.
	if ( !IsDormant() && bDormant && GetOwner() && !IsCarriedByLocalPlayer() )
	{
		Holster( NULL );
	}

	BaseClass::SetDormant( bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit(state);

	if (state == SHOULDTRANSMIT_END)
	{
		if (m_iState == WEAPON_IS_ACTIVE)
		{
			m_iState = WEAPON_IS_CARRIED_BY_PLAYER;
		}
	}
	else if( state == SHOULDTRANSMIT_START )
	{
		if( m_iState == WEAPON_IS_CARRIED_BY_PLAYER )
		{
			if( GetOwner() && GetOwner()->GetActiveWeapon() == this )
			{
				// Restore the Activeness of the weapon if we client-twiddled it off in the first case above.
				m_iState = WEAPON_IS_ACTIVE;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: To wrap PORTAL mod specific functionality into one place
//-----------------------------------------------------------------------------
static inline bool ShouldDrawLocalPlayerViewModel( void )
{
#if defined( PORTAL ) && !defined( GAMMA_PORTALGUN )
	return false;
#else
	// Gamma retains HL2's server viewmodel animations when merging the portal gun.
	// Replacing those local weapon models with world models leaves invalid sequence indices.
	return !C_BasePlayer::ShouldDrawLocalPlayer();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::OnRestore()
{
	BaseClass::OnRestore();

	// if the player is holding this weapon, 
	// mark it as just restored so it won't show as a new pickup
	if (GetOwner() == C_BasePlayer::GetLocalPlayer())
	{
		m_bJustRestored = true;
	}
}

int C_BaseCombatWeapon::GetWorldModelIndex( void )
{
	if ( GameRules() )
	{
		const char *pBaseName = modelinfo->GetModelName( modelinfo->GetModel( m_iWorldModelIndex ) );
		const char *pTranslatedName = GameRules()->TranslateEffectForVisionFilter( "weapons", pBaseName );

		if ( pTranslatedName != pBaseName )
		{
			return modelinfo->GetModelIndex( pTranslatedName );
		}
	}

	return m_iWorldModelIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged(updateType);

	CHandle< C_BaseCombatWeapon > handle = this;

	// If it's being carried by the *local* player, on the first update,
	// find the registered weapon for this ID

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseCombatCharacter *pOwner = GetOwner();

	// check if weapon is carried by local player
	bool bIsLocalPlayer = pPlayer && pPlayer == pOwner;
	if ( bIsLocalPlayer
#ifndef GAMMA_PORTALGUN
		&& ShouldDrawLocalPlayerViewModel()
#endif
		)
	{
		// If I was just picked up, or created & immediately carried, add myself to this client's list of weapons
		if ( (m_iState != WEAPON_NOT_CARRIED ) && (m_iOldState == WEAPON_NOT_CARRIED) )
		{
			// Tell the HUD this weapon's been picked up
			if ( ShouldDrawPickup() )
			{
				CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
				if ( pHudSelection )
				{
					pHudSelection->OnWeaponPickup( this );
				}

				pPlayer->EmitSound( "Player.PickupWeapon" );
			}
		}
	}
	else // weapon carried by other player or not at all
	{
		int overrideModelIndex = CalcOverrideModelIndex();
		if( overrideModelIndex != -1 && overrideModelIndex != GetModelIndex() )
		{
			SetModelIndex( overrideModelIndex );
		}
	}

	UpdateVisibility();

	m_iOldState = m_iState;

	m_bJustRestored = false;
}

//-----------------------------------------------------------------------------
// Is anyone carrying it?
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsBeingCarried() const
{
	return ( m_hOwner.Get() != NULL );
}

//-----------------------------------------------------------------------------
// Is the carrier alive?
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsCarrierAlive() const
{
	if ( !m_hOwner.Get() )
		return false;

	return m_hOwner.Get()->GetHealth() > 0;
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_BaseCombatWeapon::ShadowCastType()
{
	if ( IsEffectActive( /*EF_NODRAW |*/ EF_NOSHADOW ) )
		return SHADOWS_NONE;

	if (!IsBeingCarried())
		return SHADOWS_RENDER_TO_TEXTURE;

	if (IsCarriedByLocalPlayer() && !C_BasePlayer::ShouldDrawLocalPlayer())
		return SHADOWS_NONE;

	return SHADOWS_RENDER_TO_TEXTURE;
}

//-----------------------------------------------------------------------------
// Purpose: This weapon is the active weapon, and it should now draw anything
//			it wants to. This gets called every frame.
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::Redraw()
{
	if ( g_pClientMode->ShouldDrawCrosshair() )
	{
		DrawCrosshair();
	}

	// ammo drawing has been moved into hud_ammo.cpp
}

//-----------------------------------------------------------------------------
// Purpose: Draw the weapon's crosshair
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::DrawCrosshair()
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	Color clr = gHUD.m_clrNormal;
/*

	// TEST: if the thing under your crosshair is on a different team, light the crosshair with a different color.
	Vector vShootPos, vShootAngles;
	GetShootPosition( vShootPos, vShootAngles );

	Vector vForward;
	AngleVectors( vShootAngles, &vForward );
	
	
	// Change the color depending on if we're looking at a friend or an enemy.
	CPartitionFilterListMask filter( PARTITION_ALL_CLIENT_EDICTS );	
	trace_t tr;
	traceline->TraceLine( vShootPos, vShootPos + vForward * 10000, COLLISION_GROUP_NONE, MASK_SHOT, &tr, true, ~0, &filter );

	if ( tr.index != 0 && tr.index != INVALID_CLIENTENTITY_HANDLE )
	{
		C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( tr.index );
		if ( pEnt )
		{
			if ( pEnt->GetTeamNumber() != player->GetTeamNumber() )
			{
				g = b = 0;
			}
		}
	}		 
*/

	CHudCrosshair *crosshair = GET_HUDELEMENT( CHudCrosshair );
	if ( !crosshair )
		return;

	// Find out if this weapon's auto-aimed onto a target
	bool bOnTarget = ( m_iState == WEAPON_IS_ONTARGET );
	
	if ( player->GetFOV() >= 90 )
	{ 
		// normal crosshairs
		if ( bOnTarget && GetWpnData().iconAutoaim )
		{
			clr[3] = 255;

			crosshair->SetCrosshair( GetWpnData().iconAutoaim, clr );
		}
		else if ( GetWpnData().iconCrosshair )
		{
			clr[3] = 255;
			crosshair->SetCrosshair( GetWpnData().iconCrosshair, clr );
		}
		else
		{
			crosshair->ResetCrosshair();
		}
	}
	else
	{ 
		Color white( 255, 255, 255, 255 );

		// zoomed crosshairs
		if (bOnTarget && GetWpnData().iconZoomedAutoaim)
			crosshair->SetCrosshair(GetWpnData().iconZoomedAutoaim, white);
		else if ( GetWpnData().iconZoomedCrosshair )
			crosshair->SetCrosshair( GetWpnData().iconZoomedCrosshair, white );
		else
			crosshair->ResetCrosshair();
	}
}

//-----------------------------------------------------------------------------
// Purpose: This weapon is the active weapon, and the viewmodel for it was just drawn.
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::ViewModelDrawn( C_BaseViewModel *pViewModel )
{
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this client's carrying this weapon
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsCarriedByLocalPlayer( void )
{
	if ( !GetOwner() )
		return false;

	return ( GetOwner() == C_BasePlayer::GetLocalPlayer() );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this client is carrying this weapon and is
//			using the view models
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::ShouldDrawUsingViewModel( void )
{
	return IsCarriedByLocalPlayer() && !C_BasePlayer::ShouldDrawLocalPlayer();
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this weapon is the local client's currently wielded weapon
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsActiveByLocalPlayer( void )
{
	if ( IsCarriedByLocalPlayer() )
	{
		return (m_iState == WEAPON_IS_ACTIVE);
	}

	return false;
}

bool C_BaseCombatWeapon::GetShootPosition( Vector &vOrigin, QAngle &vAngles )
{
	// Get the entity because the weapon doesn't have the right angles.
	C_BaseCombatCharacter *pEnt = ToBaseCombatCharacter( GetOwner() );
	if ( pEnt )
	{
		if ( pEnt == C_BasePlayer::GetLocalPlayer() )
		{
			vAngles = pEnt->EyeAngles();
		}
		else
		{
			vAngles = pEnt->GetRenderAngles();	
		}
	}
	else
	{
		vAngles.Init();
	}

	QAngle vDummy;
	if ( IsActiveByLocalPlayer() && ShouldDrawLocalPlayerViewModel() )
	{
		C_BasePlayer *player = ToBasePlayer( pEnt );
		C_BaseViewModel *vm = player ? player->GetViewModel( 0 ) : NULL;
		if ( vm )
		{
			int iAttachment = vm->LookupAttachment( "muzzle" );
			if ( vm->GetAttachment( iAttachment, vOrigin, vDummy ) )
			{
				return true;
			}
		}
	}
	else
	{
		// Thirdperson
		int iAttachment = LookupAttachment( "muzzle" );
		if ( GetAttachment( iAttachment, vOrigin, vDummy ) )
		{
			return true;
		}
	}

	vOrigin = GetRenderOrigin();
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::ShouldDraw( void )
{
	if ( m_iWorldModelIndex == 0 )
		return false;

	// FIXME: All weapons with owners are set to transmit in CBaseCombatWeapon::UpdateTransmitState,
	// even if they have EF_NODRAW set, so we have to check this here. Ideally they would never
	// transmit except for the weapons owned by the local player.
	if ( IsEffectActive( EF_NODRAW ) )
		return false;

	C_BaseCombatCharacter *pOwner = GetOwner();

	// weapon has no owner, always draw it
	if ( !pOwner )
		return true;

	bool bIsActive = ( m_iState == WEAPON_IS_ACTIVE );

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	 // carried by local player?
	if ( pOwner == pLocalPlayer )
	{
		// Only ever show the active weapon
		if ( !bIsActive )
			return false;

#ifdef GAMMA_PORTALGUN
		// Visibility is collected before portal recursion begins. Keep the active
		// weapon in the render list and decide which view may draw it in DrawModel.
		return pOwner->ShouldDraw();
#endif

		if ( !pOwner->ShouldDraw() )
		{
			// Our owner is invisible.
			// This also tests whether the player is zoomed in, in which case you don't want to draw the weapon.
			return false;
		}

		// 3rd person mode?
		if ( !ShouldDrawLocalPlayerViewModel() )
			return true;

		// don't draw active weapon if not in some kind of 3rd person mode, the viewmodel will do that
		return false;
	}

	// If it's a player, then only show active weapons
	if ( pOwner->IsPlayer() )
	{
		// Show it if it's active...
		return bIsActive;
	}

	// FIXME: We may want to only show active weapons on NPCs
	// These are carried by AIs; always show them
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if a weapon-pickup icon should be displayed when this weapon is received
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::ShouldDrawPickup( void )
{
	if ( GetWeaponFlags() & ITEM_FLAG_NOITEMPICKUP )
		return false;

	if ( m_bJustRestored )
		return false;

	return true;
}
		   
//-----------------------------------------------------------------------------
// Purpose: Render the weapon. Draw the Viewmodel if the weapon's being carried
//			by this player, otherwise draw the worldmodel.
//-----------------------------------------------------------------------------
int C_BaseCombatWeapon::DrawModel( int flags )
{
	VPROF_BUDGET( "C_BaseCombatWeapon::DrawModel", VPROF_BUDGETGROUP_MODEL_RENDERING );
	if ( !m_bReadyToDraw )
		return 0;

	if ( !IsVisible() )
		return 0;

	// check if local player chases owner of this weapon in first person
	C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer();

	if ( localplayer && localplayer->IsObserver() && GetOwner() )
	{
		// don't draw weapon if chasing this guy as spectator
		// we don't check that in ShouldDraw() since this may change
		// without notification 
		
		if ( localplayer->GetObserverMode() == OBS_MODE_IN_EYE &&
			 localplayer->GetObserverTarget() == GetOwner() ) 
			return false;
	}

#ifdef GAMMA_PORTALGUN
	if ( IsCarriedByLocalPlayer() && ShouldDrawLocalPlayerViewModel() )
	{
		if ( !g_pPortalRender->IsRenderingPortal() ) return 0;
		C_Portal_Player *player = C_Portal_Player::GetLocalPortalPlayer();
		if ( player && g_pPortalRender->GetViewRecursionLevel() == 1 && player->IsHiddenInPortalView() ) return 0;
	}
	CGammaWeaponWorldModelScope worldModel( GetOwner() && GetOwner()->IsPlayer() ? this : NULL );
#endif

	return BaseClass::DrawModel( flags );
}


//-----------------------------------------------------------------------------
// Allows the client-side entity to override what the network tells it to use for
// a model. This is used for third person mode, specifically in HL2 where the
// the weapon timings are on the view model and not the world model. That means the
// server needs to use the view model, but the client wants to use the world model.
//-----------------------------------------------------------------------------
int C_BaseCombatWeapon::CalcOverrideModelIndex() 
{ 
	C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer();
	if ( localplayer && 
		localplayer == GetOwner()
#ifndef GAMMA_PORTALGUN
		&& ShouldDrawLocalPlayerViewModel()
#endif
		)
	{
		// Keep the prediction model independent of the current render view. The
		// scoped world-model draw handles both portals and third person.
		return BaseClass::CalcOverrideModelIndex();
	}
	else
	{
		return GetWorldModelIndex();
	}
}

#ifdef GAMMA_PORTALGUN
void C_BaseCombatWeapon::BuildTransformations( CStudioHdr *header, Vector *pos, Quaternion q[], const matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	BaseClass::BuildTransformations( header, pos, q, cameraTransform, boneMask, boneComputed );
	C_BaseCombatCharacter *owner = GetOwner();
	if ( !header || header->numbones() == 0 || !owner || !owner->IsPlayer() || GetModelIndex() != GetWorldModelIndex() ) return;
	const char *ownerModel = modelinfo->GetModelName( owner->GetModel() );
	if ( !ownerModel || Q_strnicmp( ownerModel, "models/gamma/player/", 20 ) ) return;
	// Properly rigged world models already merge their right hand onto the
	// player. Several original 2003 pickups have only an arbitrary root bone.
	if ( LookupBone( "ValveBiped.Bip01_R_Hand" ) >= 0 ) return;
	int hand = owner->LookupBone( "ValveBiped.Bip01_R_Hand" );
	if ( hand < 0 || !owner->SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime ) ) return;
	matrix3x4_t inverseRoot, correction, grip, root;
	SetIdentityMatrix( grip );
	if ( LookupBone( "weapon_bone" ) >= 0 && !Q_stricmp( GetWorldModel(), "models/weapons/w_portalgun.mdl" ) )
	{
		// Original Chell's wrist-to-weapon transform, converted between the
		// Portal and ValveBiped palm axes. A bare hand transform points the
		// portal gun's -Z barrel vertically instead of along the fingers.
		const matrix3x4_t portalGrip( 0.01246916f, 0.12911768f, -0.99155089f, 0.03754750f,
			0.18316660f, 0.97455390f, 0.12920777f, 0.46297575f,
			0.98300281f, -0.18323011f, -0.01149818f, 0.28804979f );
		MatrixCopy( portalGrip, grip );
	}
	else if ( !Q_stricmp( GetWorldModel(), "models/weapons/w_immolator.mdl" ) )
	{
		// The legacy Bone01 bind rotates its model-space -Y barrel onto local
		// +Z. Turn that axis onto the ValveBiped hand's forward (+X) axis.
		// Keep the original grip pivot so its handle remains in the palm.
		const matrix3x4_t immolatorGrip( 0, 0, 1, 0,
			0, 1, 0, 0,
			-1, 0, 0, 0 );
		MatrixCopy( immolatorGrip, grip );
	}
	else if ( !Q_stricmp( GetWorldModel(), "models/weapons/w_physics_gun.mdl" ) ||
		!Q_stricmp( GetWorldModel(), "models/gamma/weapons/w_early_physgun.mdl" ) )
	{
		// Keep the 2001-2002 gun geometry. Its original animated view-model
		// reference supplies the grip; coincident X/Z vertices establish the
		// world-to-view translation before normalizing the old wrist axes.
		const matrix3x4_t earlyPhysgunGrip( -0.15391994f, -0.98444332f, 0.08473493f, gamma_physgun_grip_x.GetFloat(),
			-0.98462955f, 0.14565292f, -0.09638390f, gamma_physgun_grip_y.GetFloat(),
			0.08254258f, -0.09826792f, -0.99173086f, gamma_physgun_grip_z.GetFloat() );
		MatrixCopy( earlyPhysgunGrip, grip );
	}
	ConcatTransforms( owner->GetBone( hand ), grip, root );
	MatrixInvert( GetBone( 0 ), inverseRoot );
	ConcatTransforms( root, inverseRoot, correction );
	for ( int bone = 0; bone < header->numbones(); ++bone )
	{
		if ( !( header->boneFlags( bone ) & boneMask ) ) continue;
		matrix3x4_t aligned;
		ConcatTransforms( correction, GetBone( bone ), aligned );
		MatrixCopy( aligned, GetBoneForWrite( bone ) );
	}
}
#endif


//-----------------------------------------------------------------------------
// tool recording
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	int nModelIndex = GetModelIndex();
	int nWorldModelIndex = GetWorldModelIndex();
	if ( nModelIndex != nWorldModelIndex )
	{
		SetModelIndex( nWorldModelIndex );
	}

	BaseClass::GetToolRecordingState( msg );

	if ( m_iState == WEAPON_NOT_CARRIED )
	{
		BaseEntityRecordingState_t *pBaseEntity = (BaseEntityRecordingState_t*)msg->GetPtr( "baseentity" );
		pBaseEntity->m_nOwner = -1;
	}
	else
	{
		msg->SetInt( "worldmodel", 1 );
		if ( m_iState == WEAPON_IS_ACTIVE )
		{
			BaseEntityRecordingState_t *pBaseEntity = (BaseEntityRecordingState_t*)msg->GetPtr( "baseentity" );
			pBaseEntity->m_bVisible = true;
		}
	}

	if ( nModelIndex != nWorldModelIndex )
	{
		SetModelIndex( nModelIndex );
	}
}
