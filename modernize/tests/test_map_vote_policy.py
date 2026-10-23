"""Native C++98 regression coverage for the independently testable vote rules."""
from pathlib import Path
import shutil, subprocess, tempfile, os, unittest
ROOT=Path(__file__).resolve().parents[1]
class VotePolicyTests(unittest.TestCase):
    def test_cpp98_map_vote(self):
        compiler=shutil.which('g++') or shutil.which('clang++')
        if not compiler: self.skipTest('No native compiler')
        with tempfile.TemporaryDirectory() as tmp:
            binary=Path(tmp)/('votetest.exe' if os.name=='nt' else 'votetest')
            subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-pedantic','-Werror',str(ROOT/'tests/map_vote_policy_smoke.cpp'),'-o',str(binary)],capture_output=True,text=True,check=True)
            proc=subprocess.run([str(binary)],capture_output=True,text=True,check=True)
            self.assertIn('map vote policy tests passed',proc.stdout)
if __name__=='__main__':unittest.main()
