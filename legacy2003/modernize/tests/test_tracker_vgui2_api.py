"""Static regression tests for known Win32 Tracker VGUI1 -> VGUI2 diagnostics.

These checks do not replace a Windows Visual C++ build or runtime test.
"""
from pathlib import Path
import hashlib
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding='latin-1')


class TrackerVGui2SourceRepair(unittest.TestCase):
    def test_missing_tracker_user_interface_has_provider_methods(self):
        header = read('common/ITrackerUser.h')
        impl = read('Tracker/TrackerUI/TrackerUser.cpp')
        for name in ['IsValid', 'GetTrackerID', 'GetUserName', 'GetFirstName',
                     'GetLastName', 'GetEmail', 'IsFriend',
                     'RequestAuthorizationFromUser', 'GetUserStatus',
                     'GetUserGameAddress', 'GetNumberOfFriends',
                     'GetFriendTrackerID', 'SetCanReceiveMessages']:
            self.assertRegex(header, r'\b'+name+r'\s*\(')
            self.assertRegex(impl, r'\b'+name+r'\s*\(')
        self.assertIn('TRACKERUSER_INTERFACE_VERSION', header)

    def test_vgui2_list_and_keyvalues_callsites(self):
        sub = ROOT / 'Tracker' / 'AdminServer'
        joined = '\n'.join(p.read_text(encoding='latin-1') for p in sub.iterdir()
                           if p.suffix.lower() in ('.h', '.cpp'))
        for invalid in ('vgui::KeyValues', 'ListPanel::DATAITEM',
                        'GetDataItem(', 'GetSelectedRow(', 'GetNumSelectedRows(',
                        'ClearMenu(', 'SetRichEdit(', 'setMaximumCharCount('):
            self.assertNotIn(invalid, joined)
        self.assertIn('vgui::ListPanelItem', joined)
        self.assertIn('GetSelectedItemsCount(', joined)

    def test_graph_point_layout_and_draw_api(self):
        point = read('Tracker/AdminServer/point.h')
        self.assertIn('#define TRACKER_ADMIN_POINT_H', point)
        self.assertIn('class Point', read('public/vgui/Point.h'))
        graph = read('Tracker/AdminServer/GraphPanel.cpp')
        header = read('Tracker/AdminServer/GraphPanel.h')
        self.assertIn('DrawPointPolyLine(const vgui::Point *vertices, int count)', header)
        self.assertIn('DrawPolyLine(xs.Base(), ys.Base(), count)', graph)
        self.assertIn('#include <vgui/Point.h>', header)

    def test_ivgui_module_handles_and_parents(self):
        for header in ('Tracker/AdminServer/AdminServer.h',
                       'Tracker/Browser/Browser.h',
                       'Tracker/hlds_gui/hlds_gui.h'):
            contents = read(header)
            self.assertIn('virtual vgui::VPANEL GetPanel();', contents)
            self.assertIn('virtual void SetParent(vgui::VPANEL parent);', contents)
            self.assertNotIn('vgui::VPanel *', contents)
        self.assertIn('CTrackerUIVGuiModule::SetParent(',
                      read('Tracker/TrackerUI/TrackerUIVGuiModule.cpp'))
        self.assertIn('virtual void Reactivate();', read('Tracker/hlds_gui/hlds_gui.h'))

    def test_admin_server_uses_existing_dialog_class(self):
        self.assertIn('class VInternetDlg', read('Tracker/AdminServer/serverpage.h'))
        for path in (ROOT/'Tracker'/'AdminServer').glob('*.cpp'):
            self.assertNotIn('CServerPage', path.read_text(encoding='latin-1'))
        self.assertIn('server.listEntryID', read('Tracker/AdminServer/FavoriteGames.cpp'))
        self.assertNotIn('server.listEntry =', read('Tracker/AdminServer/MasterMsgHandler.cpp'))

    def test_regeneration_script_runs_twice_without_changes(self):
        script = ROOT/'modernize'/'repair_tracker_vgui2_api.py'
        files = [script, ROOT/'Tracker/AdminServer/BaseGamesPage.cpp',
                 ROOT/'Tracker/Browser/Browser.h',
                 ROOT/'Tracker/AdminServer/GraphPanel.cpp']
        before = [hashlib.sha256(p.read_bytes()).hexdigest() for p in files]
        for i in range(2):
            result = subprocess.run([sys.executable, str(script)], cwd=ROOT,
                                    text=True, capture_output=True, check=True)
            self.assertIn('files modified: 0', result.stdout)
        self.assertEqual(before, [hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in files])

    def test_shared_forward_declarations_present(self):
        for path, name in [('public/vgui_controls/Label.h','TextImage'),
                           ('public/vgui_controls/Panel.h','BuildGroup'),
                           ('public/vgui_controls/ListPanel.h','ImageList')]:
            hdr = read(path)
            self.assertRegex(hdr, r'(?m)^class '+name+r';\s*$')

if __name__ == '__main__':
    unittest.main()
