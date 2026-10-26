#!/usr/bin/env python3
"""Add opt-in deathmatch spawn selection that avoids nearby live opponents.

Uses the original deathmatch spawn validation and leaves the original selection
code available with mp_fair_spawns 0. Does not alter single-player spawning.
"""
from pathlib import Path

p = Path(__file__).resolve().parents[1] / 'dlls' / 'player.cpp'
s = p.read_bytes().decode('latin-1').replace('\r\n', '\n')
if 'HL2MODERN_FAIR_SPAWNS' not in s:
    point = 'CBaseEntity\t*g_pLastSpawn = NULL;'
    assert s.count(point) == 1
    s = s.replace(point, '''// HL2MODERN_FAIR_SPAWNS
// Prefer valid deathmatch spawn points farther from living opponents.
// A server can restore the legacy randomized behavior with mp_fair_spawns 0.
static ConVar mp_fair_spawns( "mp_fair_spawns", "1", FCVAR_SERVER,
    "Select valid DM spawn farthest from the nearest living opponent." );

''' + point)
    point='''	else if ( g_pGameRules->IsDeathmatch() )
	{
		pSpot = g_pLastSpawn;'''
    assert s.count(point) == 1
    replacement='''\telse if ( g_pGameRules->IsDeathmatch() )
\t{
        // HL2MODERN_FAIR_SPAWNS: consider all viable deathmatch spawn
        // points instead of immediately taking a random, camped location.
        // Do not choose a spot overlapping another client.
        if ( mp_fair_spawns.GetBool() )
        {
            CBaseEntity *pCandidate = NULL;
            CBaseEntity *pBest = NULL;
            float bestNearestDistance = -1.0f;
            while ( (pCandidate = gEntList.FindEntityByClassname(
                         pCandidate, "info_player_deathmatch")) != NULL )
            {
                if ( pCandidate->GetAbsOrigin() == vec3_origin ||
                     !IsSpawnPointValid( this, pCandidate ) )
                    continue;
                float nearestOpponentDistance = 1.0e30f;
                for ( int slot = 1; slot <= gpGlobals->maxClients; ++slot )
                {
                    CBasePlayer *pOther = UTIL_PlayerByIndex(slot);
                    if ( !pOther || pOther == this || !pOther->IsAlive() )
                        continue;
                    if ( g_pGameRules->IsTeamplay() &&
                         pOther->GetTeamNumber() == GetTeamNumber() )
                        continue;
                    Vector difference = pCandidate->GetAbsOrigin() - pOther->GetAbsOrigin();
                    float distanceSquared = difference.LengthSqr();
                    if ( distanceSquared < nearestOpponentDistance )
                        nearestOpponentDistance = distanceSquared;
                }
                if ( !pBest || nearestOpponentDistance > bestNearestDistance )
                {
                    pBest = pCandidate;
                    bestNearestDistance = nearestOpponentDistance;
                }
            }
            if ( pBest )
            {
                pSpot = pBest;
                goto ReturnSpot;
            }
        }

\t\tpSpot = g_pLastSpawn;'''
    s = s.replace(point,replacement)
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('Deathmatch fair-spawn policy patched')
