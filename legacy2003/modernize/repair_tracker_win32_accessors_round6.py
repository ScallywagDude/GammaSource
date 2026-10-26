#!/usr/bin/env python3
"""Complete missing Win32 Tracker VGUI accessor and interface declarations.

This is narrowly scoped to legacy Tracker. No changes to the VGUI2 ABI,
AMD64 build options, or the engine/gameplay source are made. Idempotent.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADMIN = ROOT / 'Tracker' / 'AdminServer'
changes = []


def apply(rel, func):
    path = ROOT / rel
    before = path.read_bytes()
    after = func(before.decode('latin1')).encode('latin1')
    if after != before:
        path.write_bytes(after)
        changes.append(rel)


def amend_base_header(t):
    marker = '\tvirtual void SetRefreshing(bool state);'
    assert marker in t, 'Unexpected AdminServer BaseGamesPage interface'
    if 'GetInvalidServerListID()' not in t:
        t = t.replace(marker, marker + '\n\tvirtual int GetInvalidServerListID();', 1)
    return t


def amend_base_cpp(t):
    signature = 'int CBaseGamesPage::GetInvalidServerListID()'
    if signature not in t:
        marker = '//-----------------------------------------------------------------------------\n// Purpose: call to let the UI now whether the game list is currently refreshing'
        assert marker in t, 'Unexpected AdminServer BaseGamesPage implementation'
        method = ('//-----------------------------------------------------------------------------\n'
                  '// Purpose: Implement the mandatory IGameList invalid-item sentinel.\n'
                  '//-----------------------------------------------------------------------------\n'
                  'int CBaseGamesPage::GetInvalidServerListID()\n'
                  '{\n'
                  '\treturn m_pGameList ? m_pGameList->InvalidItemID() : -1;\n'
                  '}\n\n'
                  '//-----------------------------------------------------------------------------\n')
        t = t.replace(marker, method + '// Purpose: call to let the UI now whether the game list is currently refreshing', 1)
    return t


def ensure_accessor_header(t):
    header = '#include <vgui_controls/Controls.h>'
    if header not in t and '#include <VGUI_Controls.h>' not in t:
        # These four translation units rely on accessor free functions; their
        # included IVGui/ISystem headers only declare the interface classes.
        lines = t.splitlines(keepends=True)
        indices = [i for i, line in enumerate(lines) if line.lstrip().startswith('#include ')]
        assert indices, 'No include anchor found'
        i = indices[-1]
        nl = '\r\n' if lines[i].endswith('\r\n') else '\n'
        lines.insert(i + 1, header + nl)
        t = ''.join(lines)
    return t


def run():
    apply('Tracker/AdminServer/BaseGamesPage.h', amend_base_header)
    apply('Tracker/AdminServer/BaseGamesPage.cpp', amend_base_cpp)
    for name in ('BaseGamesPage.cpp', 'DialogGameInfo.cpp', 'InternetGames.cpp', 'ServerContextMenu.cpp'):
        apply('Tracker/ServerBrowser/' + name, ensure_accessor_header)
    print('Tracker Win32 round6 files modified:', len(changes))
    for path in changes:
        print('  ', path)
    return changes


if __name__ == '__main__':
    run()
