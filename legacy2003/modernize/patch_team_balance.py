#!/usr/bin/env python3
"""Bound legacy team arrays and opt-in fair manual team switching.

Team-size evaluation is server side; set mp_team_balance 0 to restore the
original unrestricted manual team changes. No clients are forcibly moved.
"""
from pathlib import Path
path=Path(__file__).resolve().parents[1]/'game_shared/teamplay_gamerules.cpp'
s=path.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_TEAM_BALANCE' not in s:
    anchor='#include "team.h"\n'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'#include "../modernize/team_balance_policy.h"\n')
    anchor='static char team_names[MAX_TEAMS][MAX_TEAMNAME_LENGTH];'
    assert s.count(anchor)==1
    s=s.replace(anchor,'''// HL2MODERN_TEAM_BALANCE
static ConVar mp_team_balance("mp_team_balance", "0", FCVAR_SERVER,
    "Limit manual team changes that worsen team size disparity (0=legacy).");

'''+anchor)
    # TeamWithFewestPlayers() used MAX_TEAMS as an incorrect upper bound for
    # player count (32), which may result in no team being selected.
    a='int minPlayers = MAX_TEAMS;'
    assert s.count(a)==1
    s=s.replace(a,'int minPlayers = gpGlobals ? gpGlobals->maxClients + 1 : 65;')
    # Cap when copying configured teams and when auto-detecting new teams.
    a='''		if ( GetTeamIndex( pName ) < 0 )
		{
			Q_strncpy( team_names[num_teams], pName, sizeof(team_names[num_teams]));'''
    assert s.count(a)==1
    s=s.replace(a,'''\t\tif ( GetTeamIndex( pName ) < 0 && num_teams < MAX_TEAMS )
\t\t{
\t\t\tQ_strncpy( team_names[num_teams], pName, sizeof(team_names[num_teams]));''')
    a='''				if ( !m_teamLimit )
				{
					// add to new team
					tm = num_teams;
					num_teams++;
					team_scores[tm] = 0;'''
    assert s.count(a)==1
    s=s.replace(a,'''\t\t\t\tif ( !m_teamLimit && num_teams < MAX_TEAMS &&
\t\t\t\t     pTeamName && pTeamName[0] )
\t\t\t\t{
\t\t\t\t\t// add to new team only if the fixed arrays have capacity
\t\t\t\t\ttm = num_teams;
\t\t\t\t\tnum_teams++;
\t\t\t\t\tteam_scores[tm] = 0;''')
    # Only manual changes are gated, not autoassignment on connection.
    a='''	// notify everyone of the team change
	if ( Q_strlen( STRING(pPlayer->pl.netname) ) > 0 )'''
    assert s.count(a)==1
    s=s.replace(a,'''\t// Keep a balanced team server balanced. If an admin has inherited
\t// unbalanced counts, permit only player-initiated moves that improve them.
\t// Unknown team names are refused after multiple teams exist in balance mode;
\t// otherwise players can evade fair sizing by creating a new team.
\tif ( mp_team_balance.GetBool() )
\t{
\t\tRecountTeams();
\t\tconst int source = GetTeamIndex( pPlayer->TeamID() );
\t\tconst int destination = GetTeamIndex( mdls );
\t\tif ( source >= 0 && source != destination && num_teams >= 2 )
\t\t{
\t\t\tint counts[MAX_TEAMS] = {0};
\t\t\tfor ( int index = 1; index <= gpGlobals->maxClients; ++index )
\t\t\t{
\t\t\t\tCBasePlayer *member = UTIL_PlayerByIndex( index );
\t\t\t\tif ( !member ) continue;
\t\t\t\tconst int team = GetTeamIndex( member->TeamID() );
\t\t\t\tif ( team >= 0 && team < MAX_TEAMS ) ++counts[team];
\t\t\t}
\t\t\tif ( destination < 0 ||
\t\t\t     !HL2ModernTeams::PermitsMove(counts, num_teams, source, destination) )
\t\t\t{
\t\t\t\tconst int clientIndex = pPlayer->entindex();
\t\t\t\tengine->SetClientKeyValue(clientIndex,
\t\t\t\t\tengine->GetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->TeamName());
\t\t\t\tengine->SetClientKeyValue(clientIndex,
\t\t\t\t\tengine->GetInfoKeyBuffer(pPlayer->edict()), "team", pPlayer->TeamName());
\t\t\t\tUTIL_SayText("That team switch would unbalance the teams.\\n", pPlayer);
\t\t\t\treturn;
\t\t\t}
\t\t}
\t}

\t// notify everyone of the team change
\tif ( Q_strlen( STRING(pPlayer->pl.netname) ) > 0 )''')
    path.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('Team size safety and optional player-initiated balance rule applied')
