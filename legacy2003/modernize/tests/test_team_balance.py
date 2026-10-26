"""Team-count memory safety and opt-in switching regression tests."""
from pathlib import Path
import subprocess, tempfile, shutil, os, unittest
ROOT=Path(__file__).resolve().parents[2]
class TeamBalanceTests(unittest.TestCase):
    def test_native_cpp98_team_balance_policy(self):
        cpp=shutil.which('g++') or shutil.which('clang++')
        if not cpp: self.skipTest('No C++ compiler installed')
        with tempfile.TemporaryDirectory() as td:
            output=Path(td)/('teams.exe' if os.name=='nt' else 'teams')
            subprocess.run([cpp,'-std=c++98','-Wall','-Wextra','-pedantic','-Werror',
               str(ROOT/'modernize/tests/team_balance_smoke.cpp'),'-o',str(output)],
               check=True, capture_output=True,text=True)
            run=subprocess.run([str(output)],capture_output=True,text=True,check=True)
            self.assertIn('team balance policy tests passed',run.stdout)
    def test_source_guardrails(self):
        code=(ROOT/'game_shared/teamplay_gamerules.cpp').read_bytes().decode('latin-1')
        self.assertEqual(code.count('// HL2MODERN_TEAM_BALANCE'),1)
        self.assertIn('num_teams < MAX_TEAMS',code)
        self.assertIn('mp_team_balance("mp_team_balance", "0"',code)
        self.assertIn('HL2ModernTeams::PermitsMove(counts, num_teams, source, destination)',code)
        self.assertIn('int minPlayers = gpGlobals ? gpGlobals->maxClients + 1 : 65;',code)
    def test_generated_config(self):
        conf=(ROOT/'modernize/configs/server_teamdeathmatch.cfg').read_text()
        self.assertIn('mp_team_balance 1',conf)
        script=(ROOT/'modernize/generate_multiplayer_testmap.py').read_text()
        self.assertIn('mp_team_balance 1',script)
        self.assertIn('patch_team_balance.py',(ROOT/'modernize/apply_modernization.py').read_text())
