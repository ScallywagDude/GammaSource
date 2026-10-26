#!/usr/bin/env python3
"""Add optional server-authoritative map voting to original HL2 game server.

Only maps recognized by the engine and comprised of safe token characters are
accepted. All vote counting happens on the server. No Steam SDK dependency.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
file=root/'dlls/gameinterface.cpp'
s=file.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_MAP_VOTE_BRIDGE' not in s:
    anchor='#include "eventlog.h"\n'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'#include "../modernize/map_vote_policy.h"\n')
    anchor='// Called any time a new level is started (after GameInit() also on level transitions within a game)\n'
    assert s.count(anchor)==1
    bridge=r'''// HL2MODERN_MAP_VOTE_BRIDGE
// Map votes are intentionally disabled until the server operator enables them.
static ConVar mp_mapvote("mp_mapvote", "0", FCVAR_SERVER,
    "Allow connected multiplayer clients to initiate votes for installed maps.");
static ConVar mp_mapvote_duration("mp_mapvote_duration", "30", FCVAR_SERVER,
    "Time allowed for a map vote (clamped to 10-120 seconds).");
static ConVar mp_mapvote_cooldown("mp_mapvote_cooldown", "120", FCVAR_SERVER,
    "Minimum seconds between map-vote calls (clamped to 0-600).");
static HL2ModernVote::Session g_HL2ModernMapVote;

// Each ballot is tied to the authoritative engine userid, not just an edict.
static int HL2Modern_OnlineVotePlayers(int online[HL2ModernVote::MAX_VOTERS])
{
    int count = 0;
    for (int i=0; i<HL2ModernVote::MAX_VOTERS; ++i) online[i] = 0;
    if (!gpGlobals) return 0;
    for (int i=1; i<=gpGlobals->maxClients && i<HL2ModernVote::MAX_VOTERS; ++i)
    {
        CBasePlayer *p = UTIL_PlayerByIndex(i);
        if (!p || !p->edict() || !p->IsNetClient()) continue;
        const int userid = engine->GetPlayerUserId(p->edict());
        if (userid <= 0) continue;
        online[i] = userid;
        ++count;
    }
    return count;
}

static void HL2Modern_PollMapVote(void)
{
    if (!g_HL2ModernMapVote.active || !gpGlobals) return;
    if (!mp_mapvote.GetBool() || !g_pGameRules || !g_pGameRules->IsMultiplayer())
    {
        g_HL2ModernMapVote.Reset();
        return;
    }
    int online[HL2ModernVote::MAX_VOTERS];
    HL2Modern_OnlineVotePlayers(online);
    const HL2ModernVote::Result result = g_HL2ModernMapVote.Check(online, gpGlobals->curtime);
    if (result == HL2ModernVote::PASSED)
    {
        // Check again at execution time: map files can be removed while voting.
        if (!engine->IsMapValid(g_HL2ModernMapVote.map))
        {
            UTIL_ClientPrintAll(HUD_PRINTTALK, "Map vote cancelled: map is no longer installed.\n");
            return;
        }
        char cmd[100];
        Q_snprintf(cmd, sizeof(cmd), "changelevel %s\n", g_HL2ModernMapVote.map);
        UTIL_ClientPrintAll(HUD_PRINTTALK, "Map vote passed; changing map.\n");
        engine->ServerCommand(cmd);
    }
    else if (result == HL2ModernVote::REJECTED)
        UTIL_ClientPrintAll(HUD_PRINTTALK, "Map vote failed; not enough remaining yes votes.\n");
    else if (result == HL2ModernVote::EXPIRED)
        UTIL_ClientPrintAll(HUD_PRINTTALK, "Map vote expired without enough yes votes.\n");
}

bool HL2Modern_HandleVoteClientCommand(CBasePlayer *player, const char *cmd)
{
    if (!FStrEq(cmd, "callvote") && !FStrEq(cmd, "vote") &&
        !FStrEq(cmd, "votestatus")) return false;
    if (!player || !player->edict() || !gpGlobals) return true;
    if (!mp_mapvote.GetBool() || !g_pGameRules || !g_pGameRules->IsMultiplayer())
    {
        ClientPrint(player, HUD_PRINTCONSOLE, "Map voting is disabled on this server.\n");
        return true;
    }
    const int slot = ENTINDEX(player->edict());
    if (slot <= 0 || slot >= HL2ModernVote::MAX_VOTERS || !player->IsNetClient()) return true;
    const int userid = engine->GetPlayerUserId(player->edict());
    if (userid <= 0) return true;
    HL2Modern_PollMapVote();
    int online[HL2ModernVote::MAX_VOTERS];
    const int eligible = HL2Modern_OnlineVotePlayers(online);
    const float now = gpGlobals->curtime;

    if (FStrEq(cmd, "callvote"))
    {
        if (engine->Cmd_Argc() != 3 || !FStrEq(engine->Cmd_Argv(1), "map"))
        {
            ClientPrint(player, HUD_PRINTCONSOLE, "Usage: callvote map <installed_map_name>\n");
            return true;
        }
        const char *map = engine->Cmd_Argv(2);
        if (!HL2ModernVote::SafeMapName(map) || !engine->IsMapValid(map))
        {
            ClientPrint(player, HUD_PRINTCONSOLE, "Invalid map name or map not installed.\n");
            return true;
        }
        if (eligible < 2)
        {
            ClientPrint(player, HUD_PRINTCONSOLE, "At least two connected players are required.\n");
            return true;
        }
        if (!g_HL2ModernMapVote.Start(map, now, mp_mapvote_duration.GetFloat(),
                                      mp_mapvote_cooldown.GetFloat()))
        {
            ClientPrint(player, HUD_PRINTCONSOLE, "A vote is already active or on cooldown.\n");
            return true;
        }
        g_HL2ModernMapVote.Cast(slot, userid, HL2ModernVote::YES, now);
        UTIL_ClientPrintAll(HUD_PRINTTALK,
            "Map vote started. Type 'vote yes' or 'vote no'.\n");
    }
    else if (FStrEq(cmd, "vote"))
    {
        if (engine->Cmd_Argc() != 2 ||
            (!FStrEq(engine->Cmd_Argv(1), "yes") && !FStrEq(engine->Cmd_Argv(1), "no")))
        {
            ClientPrint(player, HUD_PRINTCONSOLE, "Usage: vote yes | vote no\n");
            return true;
        }
        const int choice = FStrEq(engine->Cmd_Argv(1), "yes") ? HL2ModernVote::YES : HL2ModernVote::NO;
        if (!g_HL2ModernMapVote.Cast(slot, userid, choice, now))
            ClientPrint(player, HUD_PRINTCONSOLE, "No active vote, or you have already voted.\n");
        else
            ClientPrint(player, HUD_PRINTCONSOLE, "Your map vote has been counted.\n");
    }
    if (g_HL2ModernMapVote.active)
    {
        HL2ModernVote::Count tally = g_HL2ModernMapVote.Tally(online);
        ClientPrint(player, HUD_PRINTCONSOLE, UTIL_VarArgs(
            "Map vote %s: yes %d / %d required, no %d, %d eligible.\n",
            g_HL2ModernMapVote.map, tally.yes, tally.needed, tally.no, tally.eligible));
    }
    else if (FStrEq(cmd, "votestatus"))
        ClientPrint(player, HUD_PRINTCONSOLE, "No map vote is currently active.\n");
    HL2Modern_PollMapVote();
    return true;
}

void HL2Modern_ResetMapVoteSlot(int slot)
{
    g_HL2ModernMapVote.ClearSlot(slot);
}

'''
    s=s.replace(anchor,bridge+anchor)
    anchor='''{
	ResetWindspeed();
'''
    assert s.count(anchor)==1
    s=s.replace(anchor,'''{
	g_HL2ModernMapVote.Reset(); // map transitions invalidate every old ballot
	ResetWindspeed();
''',1)
    anchor='''	GameStartFrame();

	Physics_RunThinkFunctions( simulating );'''
    assert s.count(anchor)==1
    s=s.replace(anchor,'''\tGameStartFrame();
	HL2Modern_PollMapVote(); // expire votes even if nobody issues a command

	Physics_RunThinkFunctions( simulating );'''.replace('\\t','\t'),1)
    anchor='''	if ( pEdict ) HL2Modern_ResetChatSlot( ENTINDEX( pEdict ) );'''
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'''
	if ( pEdict ) HL2Modern_ResetMapVoteSlot( ENTINDEX( pEdict ) );''')
    anchor='''	HL2Modern_ResetChatSlot(ENTINDEX(pEntity));'''
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'''
	HL2Modern_ResetMapVoteSlot(ENTINDEX(pEntity));''')
    file.write_bytes(s.replace('\n','\r\n').encode('latin-1'))

file=root/'dlls/client.cpp'
s=file.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2Modern_HandleVoteClientCommand' not in s:
    anchor='void ClientCommand( CBasePlayer *pPlayer )\n{'
    assert s.count(anchor)==1
    s=s.replace(anchor,'''// Optional server-authoritative multiplayer map-vote bridge.
extern bool HL2Modern_HandleVoteClientCommand(CBasePlayer *player, const char *cmd);

void ClientCommand( CBasePlayer *pPlayer )
{''')
    anchor='''	// Is the client spawned yet?
	if ( !pPlayer )
		return;
'''
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'''
	if ( HL2Modern_HandleVoteClientCommand(pPlayer, pcmd) ) return;
''')
    file.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('Map-vote command bridge and lifecycle hooks in place')

# HL2MODERN_V5_VOTE_UPGRADE: safe to apply to an already-patched v4 tree.
p=root/'dlls/gameinterface.cpp'
s=p.read_bytes().decode('latin-1').replace('\r\n','\n')
a='    else if (result == HL2ModernVote::EXPIRED)\n        UTIL_ClientPrintAll(HUD_PRINTTALK, "Map vote expired without enough yes votes.\\n");'
b='    else if (result == HL2ModernVote::REJECTED)\n        UTIL_ClientPrintAll(HUD_PRINTTALK, "Map vote failed; not enough remaining yes votes.\\n");\n    else if (result == HL2ModernVote::EXPIRED)\n        UTIL_ClientPrintAll(HUD_PRINTTALK, "Map vote expired without enough yes votes.\\n");'
if a in s and 'else if (result == HL2ModernVote::REJECTED)' not in s:
    s=s.replace(a,b)
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
