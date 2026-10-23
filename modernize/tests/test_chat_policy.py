"""Compile and execute the actual C++98 chat throttling/sanitation policy."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class ChatPolicyTests(unittest.TestCase):
    def test_cpp98_chat_policy(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if compiler is None:
            self.skipTest('g++/clang++ unavailable: optional native unit test')
        with tempfile.TemporaryDirectory() as d:
            out = pathlib.Path(d) / ('chat.exe' if __import__('os').name == 'nt' else 'chat')
            cmd = [compiler, '-std=c++98', '-Wall', '-Wextra', '-Werror', '-pedantic',
                   str(ROOT / 'tests' / 'chat_policy_smoke.cpp'), '-o', str(out)]
            subprocess.run(cmd, check=True, capture_output=True, text=True)
            test = subprocess.run([str(out)], check=True, capture_output=True, text=True)
            self.assertIn('chat policy tests passed', test.stdout)


if __name__ == '__main__':
    unittest.main()
