#!/usr/bin/env python3
"""v19: safe HL2 player asset preflight and 3D preview controls.

Run after the v18 patches. Source transforms are intentionally idempotent,
fail on an unexpected base, and preserve the original line endings.
"""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def patch(rel, changes, marker):
    file = ROOT / rel
    raw = file.read_bytes()
    crlf = b'\r\n' in raw
    s = raw.decode('latin1').replace('\r\n', '\n')
    if marker in s:
        return
    for old, new in changes:
        if s.count(old) != 1:
            raise RuntimeError('%s expected one occurrence of %r; got %s' % (rel, old[:90], s.count(old)))
        s = s.replace(old, new, 1)
    file.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin1'))

patch('dlls/hl2_dll/hl2_player.cpp', [
    ('#include "../../modernize/hl2_playermodel_policy.h"',
     '#include "../../modernize/hl2_playermodel_policy.h"\n#include "../../modernize/hl2_player_asset_guard.h"\n#include "studio.h"'),
    ('void CHL2_Player::Precache( void )\n{', '''// HL2MODERN_PLAYER_ASSET_GUARD_V19: only load a validated v36 studio
// header. A matching filename alone does not establish 2003 compatibility.
static bool HL2Modern_CompatiblePlayerModel(const char *path)
{
    if (!HL2ModernPlayerModels::Find(path) || !filesystem) return false;
    FileHandle_t file = filesystem->Open(path, "rb", "GAME");
    if (file == FILESYSTEM_INVALID_HANDLE) return false;
    unsigned char prefix[80];
    const int bytes = filesystem->Read(prefix, sizeof(prefix), file);
    const int fileLength = filesystem->Size(file);
    filesystem->Close(file);
    return fileLength > 0 && HL2ModernPlayerAssets::StudioHeader(
        prefix, bytes, (unsigned long)fileLength, STUDIO_VERSION, sizeof(studiohdr_t));
}

void CHL2_Player::Precache( void )
{'''),
    ('if (filesystem->FileExists(path)) engine->PrecacheModel(path);',
     'if (HL2Modern_CompatiblePlayerModel(path)) engine->PrecacheModel(path);'),
    ('''\tconst char *path="models/player.mdl";
\tif (edict())
\t{
\t\tchar *info=engine->GetInfoKeyBuffer(edict());
\t\tconst char *requested=info ? engine->InfoKeyValue(info,"model") : NULL;
\t\tconst HL2ModernPlayerModels::Entry *entry=HL2ModernPlayerModels::Find(requested);
\t\tif (entry && filesystem->FileExists(entry->path)) path=entry->path;
\t}
\tSetModel(path);
    g_ulModelIndexPlayer = GetModelIndex();''',
     '''\tconst char *path=NULL;
\tif (edict())
\t{
\t\tchar *info=engine->GetInfoKeyBuffer(edict());
\t\tconst char *requested=info ? engine->InfoKeyValue(info,"model") : NULL;
\t\tconst HL2ModernPlayerModels::Entry *entry=HL2ModernPlayerModels::Find(requested);
\t\tif (entry && HL2Modern_CompatiblePlayerModel(entry->path)) path=entry->path;
\t}
\t// If the requested asset is missing/unsupported, prefer the classic model,
\t// then another compatible entry. Never pass a retail v37 model into a v36 loader.
\tif (!path && HL2Modern_CompatiblePlayerModel("models/player.mdl"))
\t\tpath="models/player.mdl";
\tif (!path)
\t{
\t\tfor (unsigned int i=0; i<HL2ModernPlayerModels::Count(); ++i)
\t\t{
\t\t\tconst char *candidate=HL2ModernPlayerModels::kModels[i].path;
\t\t\tif (HL2Modern_CompatiblePlayerModel(candidate)) { path=candidate; break; }
\t\t}
\t}
\tif (path)
\t{
\t\tSetModel(path);
\t\tg_ulModelIndexPlayer = GetModelIndex();
\t}
\telse
\t{
\t\tWarning("No compatible 2003 player model installed; player appearance unavailable\\n");
\t\tg_ulModelIndexPlayer = 0;
\t\tSetModelIndex(0); // fail closed; runtime still requires a compatible asset
\t}''')
], 'HL2MODERN_PLAYER_ASSET_GUARD_V19')

patch('gameui/PlayerModelPreviewPanel.h', [
    ('    virtual void OnMousePressed(vgui::MouseCode code);',
     '    virtual void OnMousePressed(vgui::MouseCode code);\n    virtual void OnMouseDoublePressed(vgui::MouseCode code);'),
    ('    bool m_Dragging;\n    bool m_HasModel;',
     '    bool m_Dragging;\n    bool m_Paused; // HL2MODERN_PREVIEW_CONTROLS_V19\n    bool m_HasModel;')
], 'HL2MODERN_PREVIEW_CONTROLS_V19')

patch('gameui/PlayerModelPreviewPanel.cpp', [
    ('    m_Dragging = false;\n    m_HasModel = false;\n    SetPaintBackgroundEnabled',
     '    m_Dragging = false;\n    m_Paused = false; // HL2MODERN_PREVIEW_CONTROLS_V19\n    m_HasModel = false;\n    SetPaintBackgroundEnabled'),
    ('bool CPlayerModelPreviewPanel::SetModel(const char *path)\n{\n    m_AnimationTime = 0;',
     'bool CPlayerModelPreviewPanel::SetModel(const char *path)\n{\n    m_Yaw = 180.0f;\n    m_Paused = false;\n    m_AnimationTime = 0;'),
    ('    m_AnimationTime += delta;',
     '    if (!m_Paused) m_AnimationTime += delta;'),
    ('void CPlayerModelPreviewPanel::OnMousePressed(MouseCode code)\n{\n    if (code != MOUSE_LEFT)',
     '''void CPlayerModelPreviewPanel::OnMousePressed(MouseCode code)
{
    if (code == MOUSE_RIGHT)
    {
        if (m_Dragging) input()->SetMouseCapture(NULL);
        m_Dragging = false;
        m_Yaw = 180.0f;
        m_Paused = false;
        m_AnimationTime = 0.0f;
        m_LastInteractionTime = system()->GetCurrentTime();
        Repaint();
        return;
    }
    if (code == MOUSE_MIDDLE)
    {
        m_Paused = !m_Paused;
        m_LastInteractionTime = system()->GetCurrentTime();
        Repaint();
        return;
    }
    if (code != MOUSE_LEFT)'''),
    ('void CPlayerModelPreviewPanel::OnMouseReleased(MouseCode code)',
     '''void CPlayerModelPreviewPanel::OnMouseDoublePressed(MouseCode code)
{
    if (code == MOUSE_LEFT) { OnMousePressed(MOUSE_RIGHT); return; }
    BaseClass::OnMouseDoublePressed(code);
}

void CPlayerModelPreviewPanel::OnMouseReleased(MouseCode code)''')
], 'HL2MODERN_PREVIEW_CONTROLS_V19')
print('v19 player-model safety and preview controls integrated')
