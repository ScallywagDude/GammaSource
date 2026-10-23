"""Source-level Win32 Tracker interface regression checks."""
import pathlib
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def src(relative):
    return (ROOT / relative).read_bytes().decode('latin1')


class TrackerWin32AccessorRepairTests(unittest.TestCase):
    def test_admin_favorites_no_longer_abstract_from_invalid_item_method(self):
        iface = src('Tracker/common/IGameList.h')
        header = src('Tracker/AdminServer/BaseGamesPage.h')
        implementation = src('Tracker/AdminServer/BaseGamesPage.cpp')
        self.assertIn('virtual int GetInvalidServerListID() = 0;', iface)
        self.assertIn('virtual int GetInvalidServerListID();', header)
        self.assertIn('int CBaseGamesPage::GetInvalidServerListID()', implementation)
        self.assertIn('m_pGameList->InvalidItemID()', implementation)

    def test_accessor_includes_in_caller_files(self):
        for name in ('BaseGamesPage', 'DialogGameInfo', 'InternetGames', 'ServerContextMenu'):
            with self.subTest(name=name):
                self.assertIn('#include <vgui_controls/Controls.h>',
                              src('Tracker/ServerBrowser/' + name + '.cpp'))

    def test_accessor_declared_by_header(self):
        controls = src('public/vgui_controls/Controls.h')
        for symbol in ('ivgui', 'system', 'surface', 'filesystem'):
            self.assertIn('*' + symbol + '();', controls)

    def test_scripts_are_repeatable(self):
        script = ROOT / 'modernize/repair_tracker_win32_accessors_round6.py'
        result = subprocess.run([sys.executable, str(script)], cwd=str(ROOT),
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('files modified: 0', result.stdout)

    def test_regeneration_pipeline_contains_repair(self):
        self.assertIn("'repair_tracker_win32_accessors_round6.py'",
                      src('modernize/apply_modernization.py'))


if __name__ == '__main__':
    unittest.main()
