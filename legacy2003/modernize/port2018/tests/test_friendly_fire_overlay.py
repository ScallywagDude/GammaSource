"""Source-level guard tests. Windows runtime verification is separate."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[4]
ENG = ROOT / 'game'


def file_text(relative):
    return (ENG / relative).read_text(encoding='latin-1')


class FriendlyFireRestoration(unittest.TestCase):
    def test_server_cvar_default_on(self):
        code = file_text('shared/hl2/hl2_gamerules.cpp')
        self.assertRegex(code, r'ConVar\s+gamma_friendlyfire\s*\(\s*"gamma_friendlyfire"\s*,\s*"1"\s*,\s*FCVAR_ARCHIVE\s*\|\s*FCVAR_NOTIFY')
        self.assertIn('#ifndef CLIENT_DLL\nConVar gamma_friendlyfire', code)

    def test_hl2_rules_preserve_other_damage_checks(self):
        code = file_text('shared/hl2/hl2_gamerules.cpp')
        section = code.split('bool CHalfLife2::AllowDamage(', 1)[1].split('bool CHalfLife2::NPC_ShouldDropHealth(',1)[0]
        self.assertIn('!gamma_friendlyfire.GetBool()', section)
        self.assertIn('PhysCannonAccountableForObject', section)
        self.assertNotIn('if ( gamma_friendlyfire.GetBool() )\n        return true;', section)
        self.assertIn('return true;', section)

    def test_friendly_npc_damage_filters_are_toggleable(self):
        code = file_text('server/ai_basenpc.cpp')
        section = code.split('bool CAI_BaseNPC::PassesDamageFilter(',1)[1].split('int CAI_BaseNPC::OnTakeDamage_Alive(',1)[0]
        self.assertIn('blockFriendlyDamage = !gamma_friendlyfire.GetBool();', section)
        self.assertIn('blockFriendlyDamage && (CapabilitiesGet() & bits_CAP_FRIENDLY_DMG_IMMUNE)', section)
        self.assertIn('ai_block_damage.GetBool()', section)
        self.assertIn('BaseClass::PassesDamageFilter( info )', section)

    def test_player_trace_retains_multiplayer_checks(self):
        code = file_text('server/player.cpp')
        section = code.split('void CBasePlayer::TraceAttack(', 1)[1].split('SetLastHitGroup(',1)[0]
        self.assertIn('blockFriendlyNPCDamage = !gamma_friendlyfire.GetBool();', section)
        self.assertIn('bits_CAP_NO_HIT_PLAYER', section)
        self.assertIn('g_pGameRules->FPlayerCanTakeDamage', section)

    def test_hl2_player_has_no_bypass_of_proxy_or_base_filter(self):
        code = file_text('server/hl2/hl2_player.cpp')
        section = code.split('bool CHL2_Player::PassesDamageFilter(',1)[1].split('void CHL2_Player::SetFlashlightEnabled(',1)[0]
        self.assertIn('!gamma_friendlyfire.GetBool()', section)
        self.assertIn('m_hPlayerProxy->PassesDamageFilter( info )', section)
        self.assertIn('BaseClass::PassesDamageFilter( info )', section)


if __name__ == '__main__':
    unittest.main()
