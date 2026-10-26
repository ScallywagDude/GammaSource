#!/usr/bin/env python3
"""Optional deathmatch spawn protection; switchable per server (default off).

Defers feature policy to a dependency-free header with native C++98 tests.
Reapplying this patch is safe on trees patched earlier in the v6 sequence.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'dlls/player.cpp'
s=p.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_SPAWN_PROTECTION' not in s:
    a='#include "world.h"\n'
    assert s.count(a)==1
    s=s.replace(a,a+'#include "../modernize/spawn_protection_policy.h"\n')
    a='static ConVar dsp_explosion_effect_duration('
    assert s.count(a)==1
    block='''// HL2MODERN_SPAWN_PROTECTION
// Opt in on multiplayer deathmatch servers. 0 keeps original HL2 damage rules.
// Maximum duration is ten seconds; protection is cancelled by attack buttons.
static ConVar mp_spawn_protection_seconds(
    "mp_spawn_protection_seconds", "0", FCVAR_SERVER,
    "Seconds of protection against other players after DM respawn (0-10). Breaks on attack.");
static HL2ModernSpawn::Guard g_HL2ModernSpawnGuard;

// Called from server join/disconnect hooks. Slots are not stable player identity.
void HL2Modern_ResetSpawnProtectionSlot(int slot)
{
    g_HL2ModernSpawnGuard.Clear(slot);
}

'''
    s=s.replace(a,block+a)
    a='''	// go take the damage first

	
	if ( !g_pGameRules->FPlayerCanTakeDamage( this, info.GetAttacker() ) )'''
    assert s.count(a)==1
    replace='''	// Only prevent immediate *enemy player* damage after a multiplayer
	// deathmatch respawn. World hazards and self-inflicted damage still apply.
	if ( g_pGameRules && g_pGameRules->IsDeathmatch() && edict() &&
	     info.GetAttacker() && info.GetAttacker() != this &&
	     info.GetAttacker()->IsPlayer() &&
	     g_HL2ModernSpawnGuard.Protected(entindex(),
	         engine->GetPlayerUserId(edict()), gpGlobals->curtime) )
	{
		return 0;
	}
	// go take the damage first

	
	if ( !g_pGameRules->FPlayerCanTakeDamage( this, info.GetAttacker() ) )'''
    s=s.replace(a,replace)
    a='''	g_pGameRules->PlayerSpawn( this );

	m_vecSmoothedVelocity = vec3_origin;'''
    assert s.count(a)==1
    s=s.replace(a,'''	g_pGameRules->PlayerSpawn( this );

	// Record a respawn only after the rules have assigned the player to the
	// level. The current engine userid stops a reused slot inheriting immunity.
	if ( g_pGameRules->IsDeathmatch() && edict() )
		g_HL2ModernSpawnGuard.Begin(entindex(),
			engine->GetPlayerUserId(edict()), gpGlobals->curtime,
			mp_spawn_protection_seconds.GetFloat());

	m_vecSmoothedVelocity = vec3_origin;''')
    a='''		// do weapon stuff
		ItemPostFrame();'''
    assert s.count(a)==1
    s=s.replace(a,'''		// Act before ItemPostFrame: a protected player loses immunity when
		// attempting an attack rather than being able to fire while immune.
		if ( g_pGameRules && g_pGameRules->IsDeathmatch() &&
		     (m_nButtons & (IN_ATTACK | IN_ATTACK2)) && edict() )
			g_HL2ModernSpawnGuard.Cancel(entindex(),
				engine->GetPlayerUserId(edict()));
		// do weapon stuff
		ItemPostFrame();''')
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))

p=root/'dlls/gameinterface.cpp'
s=p.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2Modern_ResetSpawnProtectionSlot' not in s:
    # Keep the same join/disconnect boundaries as the existing chat/vote resets.
    a='extern void HL2Modern_ResetChatSlot(int slot);'
    assert s.count(a)==1
    s=s.replace(a,a+'\nextern void HL2Modern_ResetSpawnProtectionSlot(int slot);')
    a='\tif ( pEdict ) HL2Modern_ResetMapVoteSlot( ENTINDEX( pEdict ) );'
    assert s.count(a)==1
    s=s.replace(a,a+'\n\tif ( pEdict ) HL2Modern_ResetSpawnProtectionSlot( ENTINDEX( pEdict ) );')
    a='\tHL2Modern_ResetMapVoteSlot(ENTINDEX(pEntity));'
    assert s.count(a)==1
    s=s.replace(a,a+'\n\tHL2Modern_ResetSpawnProtectionSlot(ENTINDEX(pEntity));')
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('Optional server-side spawn protection wired')
