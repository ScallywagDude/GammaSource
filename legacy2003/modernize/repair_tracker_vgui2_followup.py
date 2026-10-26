#!/usr/bin/env python3
"""Targeted Win32 Tracker VGUI2 follow-up repairs from 2026-10-23 diagnostics.

Run *after* repair_tracker_vgui2_api.py. Preserves source encodings, is
idempotent, and does not modify AMD64, Phase 7, or shared VGUI ABI.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
ADMIN = ROOT / 'Tracker' / 'AdminServer'
changed = []


def patch(rel, func):
    path = ROOT / rel
    original = path.read_bytes()
    fixed = func(original.decode('latin1'))
    if fixed != original.decode('latin1'):
        path.write_bytes(fixed.encode('latin1'))
        changed.append(rel)


def ensure_include(t, header, anchor):
    line = '#include ' + header
    if line not in t:
        assert anchor in t, anchor
        t = t.replace(anchor, anchor + '\n' + line, 1)
    return t


def admin_header(t):
    # VGUI's KeyValues is a global class, not a vgui2 class.
    # Incomplete vgui2::KeyValues declarations shadow the real KeyValues.
    # These old files also sometimes duplicate the declaration.
    m = re.search(r'namespace vgui\s*\{.*?\n\}', t, re.S)
    if m:
        content = re.sub(r'(?m)^\s*class KeyValues;\s*\n', '\n', m.group())
        t = t[:m.start()] + content + t[m.end():]
    return t


def base_header(t):
    t = admin_header(t)
    # Cannot subclass a forward-declared base class. This single missing
    # include cascades into every OurListPanel member lookup.
    t = ensure_include(t, '<VGUI_ListPanel.h>', '#include <VGUI_Frame.h>')
    return t


def base_cpp(t):
    # Button::SetCommand(KeyValues *) takes one argument. Action signal
    # targets are installed separately using AddActionSignalTarget().
    return t.replace('SetCommand(new KeyValues("Manage"), GetVPanel())',
                     'SetCommand(new KeyValues("Manage"))')


def serverpage(t):
    # This AdminServer module defines VInternetDlg in serverpage.h, not in
    # the unrelated ServerBrowser/hlds_gui vinternetdlg.h.
    return t.replace('#include "vinternetdlg.h"', '#include "serverpage.h"')


def gameinfo(t):
    # m_pServerChatPanel/ m_pMOTDPanel are CChatPanel/CMOTDPanel,
    # not VGUI2 TextEntry widgets. They implement DoInsertString.
    t = t.replace('m_pServerChatPanel->InsertString(', 'm_pServerChatPanel->DoInsertString(')
    t = t.replace('m_pMOTDPanel->InsertString(', 'm_pMOTDPanel->DoInsertString(')
    return t


def admin_cpp(t):
    return t.replace('vgui::ipanel()->Client(vpanel)->GetPanel()',
                     'vgui::ipanel()->GetPanel(vpanel, vgui::GetControlsModuleName())')


def buddybutton(t):
    # The IPanel::Client legacy API is absent from VGUI2. Use the
    # same-module checked GetPanel, and handle a foreign/null target safely.
    t = t.replace('if (ipanel()->Client(mouseOver)->RequestInfo(keyVal))',
                  'if (ipanel()->RequestInfo(mouseOver, keyVal))')
    t = t.replace('Panel *mouseOver = ipanel()->Client(imouseOver)->GetPanel();',
                  'Panel *mouseOver = ipanel()->GetPanel(imouseOver, vgui::GetControlsModuleName());')
    t = t.replace('if (mouseOver->RequestInfo(keyVal))',
                  'if (mouseOver && mouseOver->RequestInfo(keyVal))')
    return t


def vector_header(t):
    return ensure_include(t, '"UtlVector.h"', '#include "netadr.h"')


def run():
    for name in sorted(ADMIN.glob('*.h')):
        rel = name.relative_to(ROOT).as_posix()
        patch(rel, base_header if name.name == 'BaseGamesPage.h' else admin_header)
    patch('Tracker/AdminServer/rcon.h', vector_header)
    patch('Tracker/AdminServer/ServerList.h', vector_header)
    patch('Tracker/AdminServer/BaseGamesPage.cpp', base_cpp)
    patch('Tracker/AdminServer/serverpage.cpp', serverpage)
    patch('Tracker/AdminServer/GamePanelInfo.cpp', gameinfo)
    patch('Tracker/AdminServer/AdminServer.cpp', admin_cpp)
    patch('Tracker/TrackerUI/BuddyButton.cpp', buddybutton)
    print('Tracker VGUI2 follow-up files modified:', len(changed))
    for rel in changed: print('  ', rel)
    return changed

if __name__ == '__main__':
    run()
