"""Static wiring checks for the engine-bound half of map voting."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
class MapVoteSourceTests(unittest.TestCase):
    def test_server_command_bridge_and_lifecycle(self):
        src=(ROOT/'dlls/gameinterface.cpp').read_bytes().decode('latin-1')
        self.assertEqual(src.count('HL2MODERN_MAP_VOTE_BRIDGE'),1)
        self.assertIn('engine->IsMapValid(map)',src)
        self.assertIn('engine->IsMapValid(g_HL2ModernMapVote.map)',src)
        self.assertIn('HL2ModernVote::SafeMapName(map)',src)
        self.assertEqual(src.count('else if (result == HL2ModernVote::REJECTED)'),1)
        self.assertIn('g_HL2ModernMapVote.Cast(slot, userid',src)
        self.assertIn('g_HL2ModernMapVote.ClearSlot(slot)',src)
        self.assertIn('HL2Modern_PollMapVote(); // expire votes',src)
        self.assertIn('g_HL2ModernMapVote.Reset(); // map transitions',src)
        self.assertIn('HL2Modern_ResetMapVoteSlot( ENTINDEX( pEdict ) );',src)
        self.assertIn('HL2Modern_ResetMapVoteSlot(ENTINDEX(pEntity));',src)
        caller=(ROOT/'dlls/client.cpp').read_bytes().decode('latin-1')
        self.assertIn('HL2Modern_HandleVoteClientCommand(pPlayer, pcmd)',caller)
    def test_opt_in_configs(self):
        for name in ('server_deathmatch.cfg','server_teamdeathmatch.cfg'):
            cfg=(ROOT/'modernize/configs'/name).read_text()
            for key in ('mp_mapvote 1','mp_mapvote_duration 30','mp_mapvote_cooldown 120'):
                self.assertIn(key,cfg)
        server=(ROOT/'dlls/gameinterface.cpp').read_bytes().decode('latin-1')
        self.assertIn('mp_mapvote("mp_mapvote", "0"',server)
    def test_clean_room_regeneration_includes_patches(self):
        runner=(ROOT/'modernize/apply_modernization.py').read_text()
        self.assertIn('patch_map_votes.py',runner)
        self.assertIn('patch_xinput_deadzone.py',runner)
if __name__=='__main__':unittest.main()
