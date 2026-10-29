#include "cbase.h"
#include "hl2_player.h"
#include "gamma_player_characters.h"
#include "filesystem.h"
#include "tier0/memdbgon.h"

static const GammaPlayerCharacter *FindCharacter( const char *value )
{
	for ( int n = 0; n < ARRAYSIZE( g_GammaPlayerCharacters ); ++n )
		if ( !Q_stricmp( value, g_GammaPlayerCharacters[n].alias ) || !Q_stricmp( value, g_GammaPlayerCharacters[n].model ) )
			return &g_GammaPlayerCharacters[n];
	return NULL;
}

bool CHL2_Player::IsGammaPlayerCharacter() const
{
	return FindCharacter( STRING( GetModelName() ) ) != NULL;
}

void CHL2_Player::UpdateGammaPlayerModel()
{
	if ( IsGammaPlayerCharacter() )
	{
		Vector forward;
		EyeVectors( &forward );
		SetViewtarget( EyePosition() + forward * 1024.0f );
	}
	const char *value = engine->GetClientConVarValue( entindex(), "gamma_player_model" );
	if ( !value || !*value ) value = "gordon";
	const GammaPlayerCharacter *character = FindCharacter( value );
	if ( !character || !filesystem->FileExists( character->model, "GAME" ) ) return;
	if ( !Q_stricmp( STRING( GetModelName() ), character->model ) ) return;
	if ( PrecacheModel( character->model ) <= 0 ) return;
	RemoveAllGestures();
	SetModel( character->model );
	ResetSequence( 0 );
	SetCycle( 0 );
	SetAnimation( PLAYER_IDLE );
}

// HL2's single-player weapons translate NPC activities. Choose player hold types
// here so every weapon gets the multiplayer rig's matching upper-body pose.
static Activity WeaponHoldBase( CBaseCombatWeapon *weapon )
{
	if ( !weapon ) return ACT_HL2MP_IDLE;
	const char *name = weapon->GetClassname();
	if ( Q_stristr( name, "pistol" ) || Q_stristr( name, "357" ) || Q_stristr( name, "alyxgun" ) || Q_stristr( name, "flaregun" ) ) return ACT_HL2MP_IDLE_PISTOL;
	if ( Q_stristr( name, "shotgun" ) ) return ACT_HL2MP_IDLE_SHOTGUN;
	if ( Q_stristr( name, "rpg" ) || Q_stristr( name, "launcher" ) ) return ACT_HL2MP_IDLE_RPG;
	if ( Q_stristr( name, "crossbow" ) || Q_stristr( name, "portalgun" ) ) return ACT_HL2MP_IDLE_CROSSBOW;
	if ( Q_stristr( name, "immolator" ) || Q_stristr( name, "extinguisher" ) ) return ACT_HL2MP_IDLE_PHYSGUN;
	if ( Q_stristr( name, "phys" ) || Q_stristr( name, "gravity" ) ) return ACT_HL2MP_IDLE_PHYSGUN;
	if ( Q_stristr( name, "crowbar" ) || Q_stristr( name, "stunstick" ) || Q_stristr( name, "knife" ) || Q_stristr( name, "iceaxe" ) || Q_stristr( name, "rollerwand" ) ) return ACT_HL2MP_IDLE_MELEE;
	if ( Q_stristr( name, "frag" ) || Q_stristr( name, "grenade" ) || Q_stristr( name, "molotov" ) || Q_stristr( name, "bugbait" ) || Q_stristr( name, "brickbat" ) || Q_stristr( name, "hopwire" ) || Q_stristr( name, "manhack" ) ) return ACT_HL2MP_IDLE_GRENADE;
	if ( Q_stristr( name, "slam" ) || Q_stristr( name, "satchel" ) || Q_stristr( name, "trip" ) ) return ACT_HL2MP_IDLE_SLAM;
	if ( Q_stristr( name, "smg" ) || Q_stristr( name, "mp5" ) ) return ACT_HL2MP_IDLE_SMG1;
	return ACT_HL2MP_IDLE_AR2;
}

static int CharacterSequence( CHL2_Player *player, Activity activity )
{
	const int current = player->GetSequence();
	// Keep an activity's chosen variant playing instead of randomly choosing a
	// different idle/run variant and restarting its cycle on every frame.
	if ( current >= 0 && player->GetActivity() == activity && player->GetSequenceActivity( current ) == activity )
		return current;
	return player->SelectWeightedSequence( activity );
}

void CHL2_Player::SetAnimation( PLAYER_ANIM event )
{
	if ( !IsGammaPlayerCharacter() || !IsAlive() || IsInAVehicle() )
	{
		BaseClass::SetAnimation( event );
		return;
	}
	const Activity hold = WeaponHoldBase( GetActiveWeapon() );
	// Each stock HL2DM hold type has seven activities in this order.
	if ( event == PLAYER_ATTACK1 || event == PLAYER_RELOAD )
	{
		Activity gesture = (Activity)( hold + ( event == PLAYER_ATTACK1 ? 4 : 5 ) );
		if ( SelectWeightedSequence( gesture ) >= 0 ) RestartGesture( gesture );
		return;
	}
	if ( event == PLAYER_DIE ) return;
	const float speed = GetAbsVelocity().Length2D();
	int offset = 0;
	if ( !( GetFlags() & FL_ONGROUND ) && GetMoveType() != MOVETYPE_NOCLIP && GetWaterLevel() < 2 ) offset = 6;
	else if ( GetFlags() & FL_DUCKING ) offset = speed > 1.0f ? 3 : 2;
	else if ( speed > 1.0f ) offset = 1;
	const Activity neutral[] = { ACT_IDLE, ACT_RUN, ACT_COVER_LOW, ACT_WALK_CROUCH, ACT_IDLE, ACT_IDLE, ACT_JUMP };
	Activity activity = GetActiveWeapon() ? (Activity)( hold + offset ) : neutral[offset];
	int sequence = CharacterSequence( this, activity );
	if ( sequence < 0 )
	{
		activity = (Activity)( ACT_HL2MP_IDLE_MELEE + offset );
		sequence = CharacterSequence( this, activity );
	}
	if ( sequence < 0 )
	{
		// Some imported bodies also expose standard NPC locomotion activities.
		activity = neutral[offset];
		sequence = CharacterSequence( this, activity );
	}
	if ( sequence < 0 ) return;
	if ( GetActivity() != activity || GetSequence() != sequence )
	{
		SetActivity( activity );
		ResetSequence( sequence );
		SetCycle( 0 );
	}
}

CON_COMMAND( gamma_character_status, "Report the selected character, sequence, weapon and hold activities." )
{
	CHL2_Player *player = dynamic_cast<CHL2_Player *>( UTIL_GetCommandClient() );
	if ( !player ) player = dynamic_cast<CHL2_Player *>( UTIL_GetLocalPlayer() );
	if ( !player ) return;
	Msg( "CHARACTER model=%s sequence=%s activity=%d cycle=%.3f weapon=%s\n", STRING( player->GetModelName() ), player->GetSequenceName( player->GetSequence() ), (int)player->GetActivity(), player->GetCycle(), player->GetActiveWeapon() ? player->GetActiveWeapon()->GetClassname() : "none" );
	Activity hold = WeaponHoldBase( player->GetActiveWeapon() );
	for ( int n = 0; n < 7; ++n ) Msg( "CHARACTER hold[%d]=%d sequence=%d\n", n, (int)hold+n, player->SelectWeightedSequence( (Activity)(hold+n) ) );
}
