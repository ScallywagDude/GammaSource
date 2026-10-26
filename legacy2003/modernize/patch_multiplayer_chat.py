#!/usr/bin/env python3
"""Harden legacy network chat and implement per-connected-user throttling.

Source file must still retain the old symbol name and original gamerules channel
checks. Header is C++98 compatible and receives standalone unit tests.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
p = root / 'dlls' / 'client.cpp'
s = p.read_bytes().decode('latin-1').replace('\r\n', '\n')
if 'HL2MODERN_MULTIPLAYER_CHAT_POLICY' not in s:
    target = '#include "globals.h"\n'
    assert s.count(target)==1
    s = s.replace(target, target+'#include "../modernize/chat_policy.h"\n')
    start = s.index('void Host_Say( edict_t *pEdict, int teamonly )')
    end = s.index('\n\nvoid ClientPrecache( void )', start)
    replacement = r'''// HL2MODERN_MULTIPLAYER_CHAT_POLICY
// Chat sanitation, text length and rate limiting are server-authoritative.
// 0 cooldown disables throttling. Rates longer than 30 seconds are clamped.
static ConVar mp_chat_cooldown("mp_chat_cooldown", "0.65", FCVAR_SERVER,
    "Minimum seconds between chat messages per connected player (0-30).");
static HL2ModernChat::Slot g_HL2ModernChatSlots[MAX_PLAYERS + 1];

void HL2Modern_ResetChatSlot(int slot)
{
    if (slot > 0 && slot <= MAX_PLAYERS)
        HL2ModernChat::Reset(g_HL2ModernChatSlots[slot]);
}

void Host_Say( edict_t *pEdict, int teamonly )
{
    if (!pEdict || !gpGlobals || engine->Cmd_Argc() == 0 || !g_pGameRules)
        return;

    const char *pcmd = engine->Cmd_Argv(0);
    const char *raw = NULL;
    char compound[256];
    if (!stricmp(pcmd, "say") || !stricmp(pcmd, "say_team"))
    {
        if (engine->Cmd_Argc() < 2) return;
        raw = engine->Cmd_Args();
    }
    else
    {
        if (engine->Cmd_Argc() >= 2)
            Q_snprintf(compound, sizeof(compound), "%s %s", pcmd, engine->Cmd_Args());
        else
            Q_snprintf(compound, sizeof(compound), "%s", pcmd);
        raw = compound;
    }

    char message[96];
    HL2ModernChat::Clean(raw, message, sizeof(message));
    if (!HL2ModernChat::HasVisibleText(message)) return;

    CBasePlayer *pPlayer = ToBasePlayer(CBaseEntity::Instance(pEdict));
    if (!pPlayer || !pPlayer->CanSpeak()) return;
    const int playerSlot = ENTINDEX(pEdict);
    if (playerSlot < 1 || playerSlot > MAX_PLAYERS) return;
    const int userid = engine->GetPlayerUserId(pEdict);
    if (!HL2ModernChat::Admit(g_HL2ModernChatSlots[playerSlot], userid,
                              gpGlobals->curtime, mp_chat_cooldown.GetFloat()))
        return;

    char text[256];
    char safeName[65];
    const char *rawName = STRING(pPlayer->pl.netname);
    HL2ModernChat::Clean(rawName ? rawName : "player", safeName, sizeof(safeName));
    if (!HL2ModernChat::HasVisibleText(safeName))
        Q_snprintf(safeName, sizeof(safeName), "player");
    const char *playerName = safeName;
    // Reserve space for the trailing newline and terminating byte.
    if (teamonly)
        Q_snprintf(text, sizeof(text), "%c(TEAM) %.80s: %s\n", 2, playerName, message);
    else
        Q_snprintf(text, sizeof(text), "%c%.80s: %s\n", 2, playerName, message);

    // Iterate authoritative player slots rather than walking dynamic entity types.
    for (int index = 1; index <= gpGlobals->maxClients; ++index)
    {
        CBasePlayer *client = UTIL_PlayerByIndex(index);
        if (!client || !client->edict() || client->edict() == pEdict || !client->IsNetClient())
            continue;
        if (teamonly && g_pGameRules->PlayerCanHearChat(client, pPlayer) != GR_TEAMMATE)
            continue;
        CSingleUserRecipientFilter filter(client);
        filter.MakeReliable();
        UserMessageBegin(filter, "SayText");
            WRITE_BYTE(playerSlot);
            WRITE_STRING(text);
        MessageEnd();
    }
    CSingleUserRecipientFilter self(pPlayer);
    self.MakeReliable();
    UserMessageBegin(self, "SayText");
        WRITE_BYTE(playerSlot);
        WRITE_STRING(text);
    MessageEnd();
    // Do not use untrusted player-controlled text as a printf format string.
    Msg("%s", text);
    UTIL_LogPrintf("\"%s\" %s \"%s\"\n", playerName,
                   teamonly ? "say_team" : "say", message);
}'''
    s = s[:start] + replacement + s[end:]
    p.write_bytes(s.replace('\n', '\r\n').encode('latin-1'))

p = root / 'dlls' / 'gameinterface.cpp'
s = p.read_bytes().decode('latin-1').replace('\r\n', '\n')
if 'HL2Modern_ResetChatSlot' not in s:
    target = 'void CServerGameClients::ClientPutInServer( edict_t *pEntity, const char *playername )\n{\n'
    assert s.count(target) == 1
    s = s.replace(target, 'extern void HL2Modern_ResetChatSlot(int slot);\n\n'+target+'\tHL2Modern_ResetChatSlot(ENTINDEX(pEntity));\n')
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('Multiplayer chat policy patched')

# Upgrade earlier overlays in place, without duplicating the main handler.
p = root / 'dlls' / 'client.cpp'
s = p.read_bytes().decode('latin-1').replace('\r\n', '\n')
original = s
s = s.replace('if (!pEdict || engine->Cmd_Argc() == 0 || !g_pGameRules)',
              'if (!pEdict || !gpGlobals || engine->Cmd_Argc() == 0 || !g_pGameRules)')
s = s.replace('''    const char *playerName = STRING(pPlayer->pl.netname);
    if (!playerName) playerName = "player";''', '''    char safeName[65];
    const char *rawName = STRING(pPlayer->pl.netname);
    HL2ModernChat::Clean(rawName ? rawName : "player", safeName, sizeof(safeName));
    if (!HL2ModernChat::HasVisibleText(safeName))
        Q_snprintf(safeName, sizeof(safeName), "player");
    const char *playerName = safeName;''')
s = s.replace('''    CBasePlayer *client = NULL;
    while (((client = gEntList.NextEntByClass((CBasePlayer *)client)) != NULL)
           && !FNullEnt(client->edict()))
    {
        if (!client->edict() || client->edict() == pEdict || !client->IsNetClient())''', '''    // Use indexed player slots, avoiding a dynamic entity cast and null-edict checks.
    for (int index = 1; index <= gpGlobals->maxClients; ++index)
    {
        CBasePlayer *client = UTIL_PlayerByIndex(index);
        if (!client || !client->edict() || client->edict() == pEdict || !client->IsNetClient())''')
if s != original:
    p.write_bytes(s.replace('\n', '\r\n').encode('latin-1'))

p = root / 'dlls' / 'gameinterface.cpp'
s = p.read_bytes().decode('latin-1').replace('\r\n', '\n')
start = s.index('void CServerGameClients::ClientDisconnect( edict_t *pEdict )\n{')
end = s.index('\n}', start)  # first closing brace at line start is method end
segment = s[start:end]
if 'HL2Modern_ResetChatSlot' not in segment:
    s = s[:start] + segment.replace('void CServerGameClients::ClientDisconnect( edict_t *pEdict )\n{',
        'void CServerGameClients::ClientDisconnect( edict_t *pEdict )\n{\n\tif ( pEdict ) HL2Modern_ResetChatSlot( ENTINDEX( pEdict ) );', 1) + s[end:]
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))

# The declaration must precede *both* disconnect and put-in-server callbacks.
p = root / 'dlls' / 'gameinterface.cpp'
s = p.read_bytes().decode('latin-1').replace('\r\n', '\n')
old = 'extern void HL2Modern_ResetChatSlot(int slot);\n\n'
if old in s:
    s = s.replace(old, '', 1)
    anchor = 'void CServerGameClients::ClientDisconnect( edict_t *pEdict )\n{'
    assert s.count(anchor) == 1
    s = s.replace(anchor, old + anchor, 1)
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
