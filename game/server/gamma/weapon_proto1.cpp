//========= GammaSource ============//
//
// weapon_proto1: the cut Episode Three "Proto1", as HL2: Aftermath brought it back.
//
// Aftermath ships no source, so this is rebuilt from what its server.dll exposes (the
// weapon_proto1_* convars, the "penetrating_bolt" trace, the Ping/Echo sounds and its
// "Object too heavy" / "Object does not have VPhysics" messages):
//   primary   - a bolt that punches through up to weapon_proto1_penetrate_depth units of
//               solid and goes off on the far side (weapon_proto1_exit_blast_radius)
//   secondary - a sonic ping: shoves the object under the crosshair (within
//               weapon_proto1_alt_fire_range, at most weapon_proto1_mass_limit kg), stirs
//               everything in weapon_proto1_ping_radius, and echoes back over
//               weapon_proto1_echo_sound_duration seconds
//
//=============================================================================//
#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "explode.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "vphysics_interface.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar weapon_proto1_rate_of_fire( "weapon_proto1_rate_of_fire", "0.75", FCVAR_CHEAT, "Seconds between Proto1 bolts" );
static ConVar weapon_proto1_mass_limit( "weapon_proto1_mass_limit", "250", FCVAR_CHEAT, "Heaviest object (kg) the Proto1 ping can shove" );
static ConVar weapon_proto1_alt_fire_range( "weapon_proto1_alt_fire_range", "512", FCVAR_CHEAT, "Reach of the Proto1 ping" );
static ConVar weapon_proto1_ping_delay( "weapon_proto1_ping_delay", "1.0", FCVAR_CHEAT, "Seconds between Proto1 pings" );
static ConVar weapon_proto1_ping_radius( "weapon_proto1_ping_radius", "384", FCVAR_CHEAT, "Radius the Proto1 ping disturbs" );
static ConVar weapon_proto1_penetrate_depth( "weapon_proto1_penetrate_depth", "64", FCVAR_CHEAT, "How much solid a Proto1 bolt can pass through" );
static ConVar weapon_proto1_exit_blast_radius( "weapon_proto1_exit_blast_radius", "128", FCVAR_CHEAT, "Radius of the blast where a Proto1 bolt comes out" );
static ConVar weapon_proto1_echo_sound_duration( "weapon_proto1_echo_sound_duration", "1.5", FCVAR_CHEAT, "How long the Proto1 ping echoes" );
static ConVar hl2eb_weapon_proto1_debug( "hl2eb_weapon_proto1_debug", "0", FCVAR_CHEAT, "Draw the Proto1 bolt traces" );

#define PROTO1_BOLT_DAMAGE		75.0f
#define PROTO1_BLAST_DAMAGE		60.0f
#define PROTO1_PING_IMPULSE		1200.0f
#define PROTO1_ECHO_COUNT		3

class CWeaponProto1 : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeaponProto1, CBaseHLCombatWeapon );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CWeaponProto1();

	virtual void Precache();
	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual void ItemPostFrame();
	virtual bool Reload() { return false; }	// no clip

private:
	void FirePenetratingBolt( CBasePlayer *pPlayer, const Vector &vecSrc, const Vector &vecDir );
	void Ping( CBasePlayer *pPlayer );

	int m_iEchoSoundsToDo;
	float m_flTimeNextEchoSound;
	float m_flEchoSoundDelay;
};

LINK_ENTITY_TO_CLASS( weapon_proto1, CWeaponProto1 );
PRECACHE_WEAPON_REGISTER( weapon_proto1 );

IMPLEMENT_SERVERCLASS_ST( CWeaponProto1, DT_WeaponProto1 )
END_SEND_TABLE()

BEGIN_DATADESC( CWeaponProto1 )
	DEFINE_FIELD( m_iEchoSoundsToDo, FIELD_INTEGER ),
	DEFINE_FIELD( m_flTimeNextEchoSound, FIELD_TIME ),
	DEFINE_FIELD( m_flEchoSoundDelay, FIELD_FLOAT ),
END_DATADESC()

CWeaponProto1::CWeaponProto1()
{
	m_bFiresUnderwater = true;
	m_iEchoSoundsToDo = 0;
	m_flTimeNextEchoSound = 0;
	m_flEchoSoundDelay = 0;
}

void CWeaponProto1::Precache()
{
	BaseClass::Precache();
	PrecacheScriptSound( "Weapon_Proto1.Single" );
	PrecacheScriptSound( "Weapon_Proto1.Ping" );
	PrecacheScriptSound( "Weapon_Proto1.Echo" );
	PrecacheScriptSound( "Weapon_Proto1.Blast" );
}

void CWeaponProto1::ItemPostFrame()
{
	BaseClass::ItemPostFrame();

	// the ping comes back a few times, fainter each time
	if ( m_iEchoSoundsToDo > 0 && gpGlobals->curtime >= m_flTimeNextEchoSound )
	{
		EmitSound( "Weapon_Proto1.Echo" );
		m_iEchoSoundsToDo--;
		m_flTimeNextEchoSound = gpGlobals->curtime + m_flEchoSoundDelay;
	}
}

void CWeaponProto1::PrimaryAttack()
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		WeaponSound( EMPTY );
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.3f;
		return;
	}

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );

	EmitSound( "Weapon_Proto1.Single" );
	pPlayer->DoMuzzleFlash();
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

	m_flNextPrimaryAttack = gpGlobals->curtime + weapon_proto1_rate_of_fire.GetFloat();
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecDir = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );
	FirePenetratingBolt( pPlayer, vecSrc, vecDir );

	pPlayer->ViewPunch( QAngle( -6, random->RandomFloat( -1, 1 ), 0 ) );
	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 600, 0.2, pPlayer );
}

//-----------------------------------------------------------------------------
// The bolt hurts what it hits, then looks for the far side of the wall: if the solid is
// thinner than weapon_proto1_penetrate_depth it bursts out there.
//-----------------------------------------------------------------------------
void CWeaponProto1::FirePenetratingBolt( CBasePlayer *pPlayer, const Vector &vecSrc, const Vector &vecDir )
{
	trace_t tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecDir * MAX_TRACE_LENGTH, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction == 1.0f )
		return;

	UTIL_Tracer( vecSrc, tr.endpos, 0, TRACER_DONT_USE_ATTACHMENT, 6000, true, "GaussTracer" );

	if ( tr.m_pEnt && tr.m_pEnt->m_takedamage != DAMAGE_NO )
	{
		CTakeDamageInfo info( this, pPlayer, PROTO1_BOLT_DAMAGE, DMG_BULLET | DMG_ENERGYBEAM | DMG_ALWAYSGIB );
		CalculateBulletDamageForce( &info, m_iPrimaryAmmoType, vecDir, tr.endpos );
		tr.m_pEnt->DispatchTraceAttack( info, vecDir, &tr );
		ApplyMultiDamage();
	}
	UTIL_ImpactTrace( &tr, DMG_BULLET );
	g_pEffects->Sparks( tr.endpos );

	// step through the solid until we are out of it
	float flDepth = weapon_proto1_penetrate_depth.GetFloat();
	Vector vecExit = vec3_invalid;
	for ( float d = 4.0f; d <= flDepth; d += 4.0f )
	{
		Vector vecTest = tr.endpos + vecDir * d;
		if ( !( UTIL_PointContents( vecTest ) & MASK_SOLID ) )
		{
			// trace back to find the exit surface
			trace_t back;
			UTIL_TraceLine( vecTest, tr.endpos, MASK_SHOT, NULL, COLLISION_GROUP_NONE, &back );
			vecExit = back.startsolid ? vecTest : back.endpos;
			break;
		}
	}

	if ( hl2eb_weapon_proto1_debug.GetBool() )
	{
		NDebugOverlay::Line( vecSrc, tr.endpos, 0, 255, 0, true, 3.0f );
		if ( vecExit != vec3_invalid )
			NDebugOverlay::Cross3D( vecExit, 8, 255, 0, 0, true, 3.0f );
	}

	if ( vecExit == vec3_invalid )
		return;

	// out the other side
	Vector vecBlast = vecExit + vecDir * 4.0f;
	EmitSound( "Weapon_Proto1.Blast" );
	g_pEffects->EnergySplash( vecBlast, -vecDir, true );
	CTakeDamageInfo blast( this, pPlayer, PROTO1_BLAST_DAMAGE, DMG_BLAST | DMG_ENERGYBEAM );
	RadiusDamage( blast, vecBlast, weapon_proto1_exit_blast_radius.GetFloat(), CLASS_NONE, NULL );
	UTIL_ScreenShake( vecBlast, 6.0f, 150.0f, 0.5f, weapon_proto1_exit_blast_radius.GetFloat() * 2, SHAKE_START );

	// keep going a little past the wall
	trace_t after;
	UTIL_TraceLine( vecBlast, vecBlast + vecDir * 1024.0f, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &after );
	UTIL_Tracer( vecBlast, after.endpos, 0, TRACER_DONT_USE_ATTACHMENT, 6000, false, "GaussTracer" );
	if ( after.m_pEnt && after.m_pEnt->m_takedamage != DAMAGE_NO )
	{
		CTakeDamageInfo info( this, pPlayer, PROTO1_BOLT_DAMAGE * 0.5f, DMG_BULLET | DMG_ENERGYBEAM );
		CalculateBulletDamageForce( &info, m_iPrimaryAmmoType, vecDir, after.endpos );
		after.m_pEnt->DispatchTraceAttack( info, vecDir, &after );
		ApplyMultiDamage();
	}
}

void CWeaponProto1::SecondaryAttack()
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	Ping( pPlayer );
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	m_flNextSecondaryAttack = gpGlobals->curtime + weapon_proto1_ping_delay.GetFloat();
	m_flNextPrimaryAttack = MAX( m_flNextPrimaryAttack, gpGlobals->curtime + 0.3f );
}

void CWeaponProto1::Ping( CBasePlayer *pPlayer )
{
	EmitSound( "Weapon_Proto1.Ping" );

	m_iEchoSoundsToDo = PROTO1_ECHO_COUNT;
	m_flEchoSoundDelay = weapon_proto1_echo_sound_duration.GetFloat() / PROTO1_ECHO_COUNT;
	m_flTimeNextEchoSound = gpGlobals->curtime + m_flEchoSoundDelay;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecDir;
	pPlayer->EyeVectors( &vecDir );

	// everything nearby hears it
	CSoundEnt::InsertSound( SOUND_COMBAT | SOUND_CONTEXT_EXPLOSION, vecSrc, weapon_proto1_ping_radius.GetInt(), 0.5f, pPlayer );

	// shove what we're pointing at
	trace_t tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecDir * weapon_proto1_alt_fire_range.GetFloat(), MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );
	CBaseEntity *pTarget = tr.m_pEnt;
	if ( !pTarget || pTarget->IsWorld() )
		return;

	if ( pTarget->IsNPC() || pTarget->IsPlayer() )
	{
		pTarget->ApplyAbsVelocityImpulse( vecDir * 400.0f + Vector( 0, 0, 150 ) );
		return;
	}

	IPhysicsObject *pPhys = pTarget->VPhysicsGetObject();
	if ( !pPhys )
	{
		if ( hl2eb_weapon_proto1_debug.GetBool() )
			DevMsg( "Object does not have VPhysics\n" );
		return;
	}
	if ( pPhys->GetMass() > weapon_proto1_mass_limit.GetFloat() )
	{
		if ( hl2eb_weapon_proto1_debug.GetBool() )
			DevMsg( "Object too heavy\n" );
		return;
	}

	pPhys->Wake();
	pPhys->ApplyForceOffset( vecDir * PROTO1_PING_IMPULSE * pPhys->GetMass() * 0.1f, tr.endpos );
}
