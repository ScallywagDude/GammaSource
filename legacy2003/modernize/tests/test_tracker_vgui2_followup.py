"""Checks for the October 23 Tracker Win32 VGUI2 follow-up source repair."""
import hashlib
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]


def src(rel):
    return (ROOT / rel).read_text(encoding='latin1')


class TrackerFollowup(unittest.TestCase):
    def test_complete_listpanel_base_header(self):
        s = src('Tracker/AdminServer/BaseGamesPage.h')
        self.assertIn('#include <VGUI_ListPanel.h>', s)
        self.assertIn('class OurListPanel: public vgui::ListPanel', s)

    def test_no_vgui_namespaced_keyvalues_shadow(self):
        import re
        for p in (ROOT / 'Tracker/AdminServer').glob('*.h'):
            s = p.read_text(encoding='latin1')
            for match in re.findall(r'namespace vgui\s*\{.*?\n\}', s, re.S):
                self.assertNotIn('class KeyValues;', match, p.name)

    def test_explicit_vector_includes(self):
        for p in ('rcon.h', 'ServerList.h'):
            self.assertIn('#include "UtlVector.h"', src('Tracker/AdminServer/' + p))

    def test_button_commands_and_modules(self):
        self.assertNotIn('SetCommand(new KeyValues("Manage"), GetVPanel())',
                         src('Tracker/AdminServer/BaseGamesPage.cpp'))
        self.assertNotIn('#include "vinternetdlg.h"',
                         src('Tracker/AdminServer/serverpage.cpp'))

    def test_control_methods_are_correct(self):
        g = src('Tracker/AdminServer/GamePanelInfo.cpp')
        self.assertIn('m_pMOTDPanel->DoInsertString(', g)
        self.assertIn('m_pServerChatPanel->DoInsertString(', g)
        self.assertNotIn('m_pServerChatPanel->InsertString(', g)
        a = src('Tracker/AdminServer/AdminServer.cpp')
        self.assertNotIn('ipanel()->Client(', a)
        self.assertIn('GetPanel(vpanel, vgui::GetControlsModuleName())', a)
        b = src('Tracker/TrackerUI/BuddyButton.cpp')
        self.assertNotIn('ipanel()->Client(', b)
        self.assertIn('ipanel()->RequestInfo(mouseOver, keyVal)', b)
        self.assertIn('mouseOver && mouseOver->RequestInfo(keyVal)', b)

    def test_script_is_idempotent(self):
        script = ROOT / 'modernize' / 'repair_tracker_vgui2_followup.py'
        paths = [ROOT / 'Tracker/AdminServer/BaseGamesPage.h',
                 ROOT / 'Tracker/AdminServer/rcon.h',
                 ROOT / 'Tracker/TrackerUI/BuddyButton.cpp']
        original = [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
        for _ in range(2):
            r = subprocess.run([sys.executable, str(script)], cwd=ROOT,
                               capture_output=True, text=True, check=True)
            self.assertIn('files modified: 0', r.stdout)
        self.assertEqual(original, [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths])


if __name__ == '__main__':
    unittest.main()
