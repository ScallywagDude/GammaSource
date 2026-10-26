#!/usr/bin/env python3
"""Narrow Win32 Tracker -> available VGUI2 API source compatibility repairs.

Does not implement historic VGUI1 APIs, certify ABI, or change Phase 7/x64.
Run after modernize/repair_tracker_source_paths.py. Idempotent.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parent.parent
TRACKER = ROOT / 'Tracker'


def update_file(path, fn):
    raw = path.read_bytes()
    # Preserve DOS source line endings and ANSI source bytes.
    txt = raw.decode('latin-1')
    out = fn(txt)
    if out == txt:
        return False
    path.write_bytes(out.encode('latin-1'))
    return True


def replace_exact(txt, old, new):
    return txt.replace(old, new)


def common_api(txt):
    # KeyValues is globally declared in public/KeyValues.h. No vgui::KeyValues exists.
    txt = txt.replace('vgui::KeyValues', 'KeyValues')
    txt = txt.replace('using vgui::KeyValues;', '')
    txt = re.sub(r'(?<![\w:])VPanel\s*\*\s*', 'vgui::VPANEL ', txt)
    # VGUI2 uses integer handle VPANEL; the old VPanel pointer is not available.
    txt = re.sub(r'vgui::VPanel\s*\*\s*', 'vgui::VPANEL ', txt)
    txt = re.sub(r'vgui::VPANEL(?=[A-Za-z_])', 'vgui::VPANEL ', txt)
    # VGUI2 ListPanelItem replaced VGUI1 ListPanel::DATAITEM.
    txt = txt.replace('vgui::ListPanel::DATAITEM', 'vgui::ListPanelItem')
    txt = re.sub(r'(?<![\w:])ListPanel::DATAITEM', 'vgui::ListPanelItem', txt)
    txt = txt.replace('GetNumSelectedRows(', 'GetSelectedItemsCount(')
    txt = txt.replace('GetSelectedRow(', 'GetSelectedItem(')
    txt = txt.replace('GetDataItem(', 'GetItemData(')
    txt = txt.replace('SetOkButtonVisible(', 'SetOKButtonVisible(')
    txt = txt.replace('setMaximumCharCount(', 'SetMaximumCharCount(')
    txt = txt.replace('RemoveAllItems(', 'DeleteAllItems(') if False else txt
    return txt


def old_gui(txt):
    txt = common_api(txt)
    txt = txt.replace('ClearMenu();', 'DeleteAllItems();')
    txt = re.sub(r'VGui_InitInterfacesList\(factorylist, factoryCount\)', 'VGui_InitInterfacesList("LegacyTracker", factorylist, factoryCount)', txt)
    txt = txt.replace('->RemoveAllItems();', '->DeleteAllItems();')
    # Original TextEntry calls used a start offset always equal to zero.
    txt = re.sub(r'(->GetText\s*\()\s*0\s*,\s*', r'\1', txt)
    txt = re.sub(r'(?m)^([ \t]*)[^\n]*->SetRichEdit\s*\([^;]*;[^\n]*\n', '', txt)
    # Explicit original callsites, not other DoInsertString methods on other classes.
    for control in ('m_pServerChatPanel', 'm_pServerRawLogPanel', 'm_pMOTDPanel', 'm_ConsoleTextEntry'):
        # In GamePanelInfo these fields are CChatPanel / CMOTDPanel wrappers,
        # not TextEntry widgets. Leave their DoInsertString methods intact.
        if 'CGamePanelInfo::' in txt and control in ('m_pServerChatPanel', 'm_pMOTDPanel'):
            continue
        txt = txt.replace(control + '->DoInsertString(', control + '->InsertString(')
    return txt


def add_forward(txt, typename):
    anchor = 'namespace vgui\n{\n'
    if anchor not in txt:
        raise ValueError('Missing VGUI namespace anchor')
    if re.search(r'(?m)^class ' + re.escape(typename) + r';\s*$', txt):
        return txt
    return txt.replace(anchor, anchor + 'class ' + typename + ';\n', 1)


def fix_graph_header(txt):
    token = '#include "point.h"'
    if token not in txt:
        raise ValueError('Missing graph include')
    txt = txt.replace(token, token + '\n#include <vgui/Point.h>', 1) if '#include <vgui/Point.h>' not in txt else txt
    if 'DrawPointPolyLine(' not in txt:
        txt = txt.replace('virtual void Paint();', 'virtual void Paint();\n\t	void DrawPointPolyLine(const vgui::Point *vertices, int count);', 1)
    return txt


def fix_graph_cpp(txt):
    # The original Point overload is absent from VGUI2 Image; convert points
    # to the separate X/Y integer arrays required by Image::DrawPolyLine.
    txt = re.sub(r'(?<![\w])DrawPolyLine\((p(?:Cpu|In|Out|FPS|Ping)),\s*points\.Count\(\)\)',
                 r'DrawPointPolyLine(\1, points.Count())', txt)
    if 'void CGraphPanel::CGraphsImage::DrawPointPolyLine' not in txt:
        anchor = 'void CGraphPanel::CGraphsImage::Paint()'
        if anchor not in txt:
            raise ValueError('Missing GraphPanel Paint entry')
        helper = '''void CGraphPanel::CGraphsImage::DrawPointPolyLine(const vgui::Point *vertices, int count)
{
    if (!vertices || count < 2)
        return;
    CUtlVector<int> xs;
    CUtlVector<int> ys;
    xs.SetCount(count);
    ys.SetCount(count);
    for (int i = 0; i < count; ++i)
        vertices[i].GetPoint(xs[i], ys[i]);
    DrawPolyLine(xs.Base(), ys.Base(), count);
}

'''
        txt = txt.replace(anchor, helper + anchor, 1)
    return txt


def fix_favorite(txt):
    # The shared serveritem_t has listEntryID, not an old pointer named listEntry.
    txt = txt.replace('server.listEntry = NULL;', 'server.listEntryID = m_pGameList->InvalidItemID();')
    txt = txt.replace('if (server.listEntry)', 'if (m_pGameList->IsValidItemID(server.listEntryID))')
    txt = txt.replace('if (!server.listEntry)', 'if (!m_pGameList->IsValidItemID(server.listEntryID))')
    txt = txt.replace('kv = server.listEntry->kv;', 'kv = m_pGameList->GetItem(server.listEntryID);')
    txt = txt.replace('server.listEntry->userData = server.serverID;', 'm_pGameList->SetUserData(server.listEntryID, server.serverID);')
    txt = txt.replace('server.listEntry = m_pGameList->GetItemData(index);', 'server.listEntryID = index;')
    txt = txt.replace('server.listEntry->row', 'server.listEntryID')
    txt = txt.replace('server.listEntry = NULL;', 'server.listEntryID = m_pGameList->InvalidItemID();')
    # Existing two-argument VGUI1 ListPanel callsites. VGUI2 needs 4 args.
    txt = txt.replace('m_pGameList->AddItem(kv, server.serverID)',
                      'm_pGameList->AddItem(kv, server.serverID, false, false)')
    return txt


def fix_admin_text(txt):
    txt = old_gui(txt)
    # This old branch names VInternetDlg "CServerPage", but no CServerPage is
    # defined; the concrete instance is VInternetDlg (serverpage.h).
    txt = re.sub(r'\bCServerPage\b', 'VInternetDlg', txt)
    return txt


def fix_base_cpp(txt):
    txt = fix_admin_text(txt)
    txt = txt.replace('scheme()->GetImage(scheme()->GetDefaultScheme(), "server/icon_password")',
                      'scheme()->GetImage("server/icon_password", false)')
    txt = txt.replace('scheme()->GetFont(scheme()->GetDefaultScheme(), "DefaultSmall")',
                      'scheme()->GetIScheme(scheme()->GetDefaultScheme())->GetFont("DefaultSmall")')
    return txt


def fix_ban_cpp(txt):
    txt = fix_admin_text(txt)
    txt = txt.replace('m_pBanListPanel->AddItem(data,userData)',
                      'm_pBanListPanel->AddItem(data,userData,false,false)')
    return txt


def fix_gameinfo_cpp(txt):
    txt = fix_admin_text(txt)
    txt = txt.replace('m_pRulesListPanel->AddItem( (*rules)[i],i)',
                      'm_pRulesListPanel->AddItem((*rules)[i],i,false,false)')
    txt = txt.replace('m_pMapsCombo->AddItem((*maps)[i].name)',
                      'm_pMapsCombo->AddItem((*maps)[i].name, NULL)')
    return txt


def fix_player_cpp(txt):
    txt = fix_admin_text(txt)
    txt = txt.replace('m_pPlayerListPanel->AddItem(kv,(*players)[i].userid)',
                      'm_pPlayerListPanel->AddItem(kv,(*players)[i].userid,false,false)')
    return txt


def fix_hlds_gui(txt):
    txt = old_gui(txt)
    return txt


def fix_browser(txt):
    return old_gui(txt)


def fix_admin_header(txt):
    return fix_admin_text(txt)


def fix_base_postmessage(txt):
    # Ensure the receiver and sender are actual VPANEL handles. Avoid
    # repeatedly prefixing the receiver on subsequent regeneration runs.
    txt = re.sub(r'(?:vgui::ivgui\(\)->)+PostMessage\(', 'vgui::ivgui()->PostMessage(', txt)
    txt = re.sub(r'(?<![\w:>])PostMessage\(m_pParent->GetVPanel\(\),',
                 'vgui::ivgui()->PostMessage(m_pParent->GetVPanel(),', txt)
    return txt


def fix_tracker_ui_module(txt):
    txt = common_api(txt)
    txt = txt.replace('VGui_InitInterfacesList(factories, factoryCount)', 'VGui_InitInterfacesList("TrackerUI", factories, factoryCount)')
    if 'virtual void SetParent(vgui::VPANEL parent);' not in txt:
        txt = txt.replace('virtual vgui::VPANEL GetPanel();',
                          'virtual vgui::VPANEL GetPanel();\n\tvirtual void SetParent(vgui::VPANEL parent);')
    if 'CTrackerUIVGuiModule::SetParent(' not in txt:
        txt += "\nvoid CTrackerUIVGuiModule::SetParent(vgui::VPANEL parent)\n{\n    if (CTrackerDialog::GetInstance())\n        CTrackerDialog::GetInstance()->SetParent(parent);\n}\n"
    return txt


def repair():
    updated = []
    def apply(rel, fn):
        path = ROOT / rel
        if not path.is_file():
            raise FileNotFoundError(path)
        if update_file(path, fn):
            updated.append(rel)

    # Missing forward declarations: they are pointer-only declarations with
    # existing classes elsewhere in the tree. Do not alter binary layout.
    for rel, typename in [('public/vgui_controls/Label.h','TextImage'),
                          ('public/vgui_controls/ListPanel.h','ImageList'),
                          ('public/vgui_controls/Panel.h','BuildGroup')]:
        apply(rel, lambda t, typ=typename: add_forward(t, typ))

    # Graph point collection declares the same POINT_H guard as public/vgui/Point.h,
    # preventing the VGUI2 Point declaration from being parsed.
    apply('Tracker/AdminServer/point.h', lambda t: t.replace('#ifndef POINT_H',
          '#ifndef TRACKER_ADMIN_POINT_H').replace('#define POINT_H', '#define TRACKER_ADMIN_POINT_H'))
    # Original Tracker port errors not addressed by generic substitutions.
    apply('Tracker/AdminServer/HelpText.cpp', lambda t: t.replace(
          'LoadFromFile(filesystem(), filename, true, "PLATFORM")',
          'LoadFromFile(filesystem(), filename, "PLATFORM")'))
    apply('Tracker/AdminServer/serverpage.cpp', lambda t: t.replace(', true, "PLATFORM")', ', "PLATFORM")'))
    apply('Tracker/AdminServer/DialogGameInfo.cpp', lambda t: t.replace(
          'GetSchemeColor("BrightControlText")',
          'GetSchemeColor("BrightControlText", scheme()->GetIScheme(scheme()->GetDefaultScheme()))'))
    apply('Tracker/AdminServer/BaseGamesPage.cpp', fix_base_postmessage)
    # Only this old Tracker implementation, not modern ServerBrowser.
    for p in (TRACKER/'AdminServer').glob('*'):
        if p.suffix.lower() in ('.cpp', '.h'):
            rel = p.relative_to(ROOT).as_posix()
            func = fix_admin_header
            if p.name == 'GraphPanel.h': func = lambda t: fix_graph_header(fix_admin_header(t))
            elif p.name == 'GraphPanel.cpp': func = lambda t: fix_graph_cpp(fix_admin_text(t))
            elif p.name == 'BaseGamesPage.cpp': func = fix_base_cpp
            elif p.name == 'FavoriteGames.cpp': func = lambda t: fix_favorite(fix_admin_text(t))
            elif p.name == 'BanPanel.cpp': func = fix_ban_cpp
            elif p.name == 'GamePanelInfo.cpp': func = fix_gameinfo_cpp
            elif p.name == 'PlayerPanel.cpp': func = fix_player_cpp
            apply(rel, func)
    apply('Tracker/AdminServer/MasterMsgHandler.cpp', lambda t: t.replace('server.listEntry = NULL;', 'server.listEntryID = -1;'))
    # Modern ServerBrowser already uses VGUI2; fix only stale KeyValues qualification.
    for p in (TRACKER/'ServerBrowser').glob('*'):
        if p.suffix.lower() in ('.cpp', '.h'):
            apply(p.relative_to(ROOT).as_posix(), lambda t: t.replace('vgui::KeyValues', 'KeyValues'))
    for sub in ('Browser', 'hlds_gui', 'TrackerUI'):
        for p in (TRACKER/sub).glob('*'):
            if p.suffix.lower() in ('.cpp', '.h'):
                apply(p.relative_to(ROOT).as_posix(), old_gui)
    # Older Tracker projects used the two-argument initializer; VGUI2 needs
    # a stable, per-binary module name for controls registration.
    for project, name in [('Browser','TrackerBrowser'),('AdminServer','TrackerAdminServer'),
                          ('hlds_gui','TrackerHLDS')]:
        apply('Tracker/'+project+'/'+{'Browser':'Browser.cpp','AdminServer':'AdminServer.cpp',
                'hlds_gui':'hlds_gui.cpp'}[project],
              lambda t, n=name: t.replace('VGui_InitInterfacesList("LegacyTracker",',
                                          'VGui_InitInterfacesList("'+n+'",'))
    # Source-API wrapper methods declared in the common IVGuiModule interface.
    for sub,cls,member in [('Browser','CBrowser','m_hInternetDlg'),
                           ('AdminServer','CAdminServer','m_hServerPage')]:
        header = ROOT / 'Tracker' / sub / (('Browser' if sub=='Browser' else 'AdminServer')+'.h')
        cpp = header.with_suffix('.cpp')
        if update_file(header, lambda t: t if 'SetParent(vgui::VPANEL' in t else t.replace(
                'virtual vgui::VPANEL GetPanel();','virtual vgui::VPANEL GetPanel();\n\tvirtual void SetParent(vgui::VPANEL parent);')):
            updated.append(header.relative_to(ROOT).as_posix())
        if update_file(cpp, lambda t: t if '::SetParent(vgui::VPANEL' in t else t+'\nvoid '+cls+'::SetParent(vgui::VPANEL parent)\n{\n    if ('+member+'.Get())\n        '+member+'->SetParent(parent);\n}\n'):
            updated.append(cpp.relative_to(ROOT).as_posix())
    # Existing Windows-only module lacks these virtuals as well; scope narrowly.
    for h, cpp, cls in [('Tracker/hlds_gui/hlds_gui.h','Tracker/hlds_gui/hlds_gui.cpp','CHLDS')]:
        apply(h, lambda t: t if 'void SetParent(vgui::VPANEL' in t else t.replace('virtual vgui::VPANEL GetPanel();', 'virtual vgui::VPANEL GetPanel();\n\tvirtual void SetParent(vgui::VPANEL parent);'))
        apply(cpp, lambda t: t if cls+'::SetParent(' in t else t+'\nvoid '+cls+'::SetParent(vgui::VPANEL parent)\n{\n    if (m_hInternetDlg.Get())\n        m_hInternetDlg->SetParent(parent);\n}\n')
    # CHLDS was missing two IVGuiModule pure virtuals.
    apply('Tracker/hlds_gui/hlds_gui.h', lambda t: t if 'virtual void Deactivate();' in t else
          t.replace('virtual void Shutdown();',
                    'virtual void Shutdown();\n\tvirtual void Deactivate();\n\tvirtual void Reactivate();'))
    apply('Tracker/hlds_gui/hlds_gui.cpp', lambda t: t if 'CHLDS::Deactivate()' in t else
          t+'\nvoid CHLDS::Deactivate() { if (m_hInternetDlg.Get()) m_hInternetDlg->SetVisible(false); }\n'
           +'void CHLDS::Reactivate() { if (m_hInternetDlg.Get()) m_hInternetDlg->SetVisible(true); }\n')
    apply('Tracker/TrackerUI/TrackerUIVGuiModule.cpp', fix_tracker_ui_module)
    print('Tracker VGUI2 source files modified:', len(updated))
    for item in updated: print(' ', item)
    return updated

if __name__ == '__main__': repair()
