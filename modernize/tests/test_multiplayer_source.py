"""Compile-independent checks for multiplayer patch injection and config defaults."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]

class MultiplayerSourceTests(unittest.TestCase):
    def test_server_chat_is_bounded_and_server_authoritative(self):
        client=(ROOT/'dlls/client.cpp').read_bytes().decode('latin-1')
        self.assertEqual(client.count('HL2MODERN_MULTIPLAYER_CHAT_POLICY'),1)
        self.assertIn('HL2ModernChat::Admit(',client)
        self.assertIn('char message[96];', client)
        self.assertIn('char safeName[65];',client)
        self.assertIn('for (int index = 1; index <= gpGlobals->maxClients; ++index)',client)
        self.assertNotIn('Msg( text );',client)
        self.assertIn('mp_chat_cooldown',client)

    def test_player_chat_slot_resets_on_join_and_leave(self):
        text=(ROOT/'dlls/gameinterface.cpp').read_bytes().decode('latin-1')
        declaration=text.index('extern void HL2Modern_ResetChatSlot(int slot);')
        disconnect=text.index('void CServerGameClients::ClientDisconnect( edict_t *pEdict )\r\n{')
        join=text.index('void CServerGameClients::ClientPutInServer( edict_t *pEntity, const char *playername )\r\n{')
        self.assertLess(declaration,disconnect)
        self.assertLess(disconnect,join)
        self.assertIn('HL2Modern_ResetChatSlot( ENTINDEX( pEdict ) );', text[disconnect:join])
        self.assertIn('HL2Modern_ResetChatSlot(ENTINDEX(pEntity));',text[join:])

    def test_server_deathmatch_spawn_policy_is_opt_out(self):
        code=(ROOT/'dlls/player.cpp').read_bytes().decode('latin-1')
        self.assertEqual(code.count('HL2MODERN_FAIR_SPAWNS'), 2)
        self.assertIn('static ConVar mp_fair_spawns(',code)
        self.assertIn('!IsSpawnPointValid( this, pCandidate )',code)
        self.assertIn('g_pGameRules->IsTeamplay()',code)
        self.assertIn('pSpot = g_pLastSpawn;',code) # legacy branch remains

    def test_local_first_server_configs(self):
        for name in ('server_deathmatch.cfg','server_teamdeathmatch.cfg'):
            config=(ROOT/'modernize/configs'/name).read_text()
            self.assertIn('sv_lan 1',config)
            self.assertIn('mp_chat_cooldown',config)
            self.assertIn('mp_fair_spawns 1',config)
        self.assertEqual((ROOT/'modernize/configs/mapcycle_modern.txt').read_text(), 'mp_modern_test\n')

if __name__ == '__main__':unittest.main()
