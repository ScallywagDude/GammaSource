#include "PlayerModelPreviewPanel.h"
#include "EngineInterface.h"
#include "IGameUIModelPreview.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/Cursor.h>
#include "../modernize/model_preview_policy.h"
#include <tier0/memdbgon.h>

using namespace vgui;

CPlayerModelPreviewPanel::CPlayerModelPreviewPanel(Panel *parent, const char *name) : Panel(parent, name)
{
    m_Yaw = 180.0f;
    m_AnimationTime = 0;
    m_LastFrameTime = system()->GetCurrentTime();
    m_LastInteractionTime = m_LastFrameTime;
    m_LastMouseX = 0;
    m_Dragging = false;
    m_Paused = false; // HL2MODERN_PREVIEW_CONTROLS_V19
    m_HasModel = false;
    SetPaintBackgroundEnabled(true);
    SetBgColor(Color(35, 39, 42, 255));
    SetKeyBoardInputEnabled(false);
    SetMouseInputEnabled(true);
    SetCursor(dc_sizewe);
}

CPlayerModelPreviewPanel::~CPlayerModelPreviewPanel()
{
    ClearModel();
}

bool CPlayerModelPreviewPanel::SetModel(const char *path)
{
    m_Yaw = 180.0f;
    m_Paused = false;
    m_AnimationTime = 0;
    m_LastFrameTime = system()->GetCurrentTime();
    m_LastInteractionTime = m_LastFrameTime;
    m_HasModel = gameuimodelpreview && gameuimodelpreview->SetModel(path);
    Repaint();
    return m_HasModel;
}

void CPlayerModelPreviewPanel::ClearModel()
{
    if (m_Dragging) input()->SetMouseCapture(NULL);
    m_Dragging = false;
    m_HasModel = false;
    if (gameuimodelpreview) gameuimodelpreview->ClearModel();
    Repaint();
}

const char *CPlayerModelPreviewPanel::GetStatusText()
{
    return gameuimodelpreview && gameuimatsurface ? gameuimodelpreview->GetStatusText() :
        "3D preview requires the updated engine and material surface";
}

void CPlayerModelPreviewPanel::OnThink()
{
    BaseClass::OnThink();
    const double now = system()->GetCurrentTime();
    const float delta = HL2ModernModelPreview::FrameDelta(now - m_LastFrameTime);
    m_LastFrameTime = now;
    if (!m_Paused) m_AnimationTime += delta;
    // Resume a slow turn after the user leaves the model alone for 2 seconds.
    if (!m_Dragging && now - m_LastInteractionTime > 2.0)
    {
        m_Yaw = HL2ModernModelPreview::WrapYaw(m_Yaw + 18.0f * delta);
    }
    if (m_Dragging && !input()->IsMouseDown(MOUSE_LEFT))
        OnMouseReleased(MOUSE_LEFT);
    if (m_HasModel) Repaint();
}

void CPlayerModelPreviewPanel::Paint()
{
    if (!m_HasModel || !gameuimodelpreview || !gameuimatsurface) return;
    int wide, tall;
    GetSize(wide, tall);
    if (wide <= 0 || tall <= 0) return;
    // Begin3DPaint in 2003 bypasses VGUI's clip rectangle. Skip a partially
    // clipped panel instead of drawing outside the Options dialog.
    int x = 0, y = 0, left, top, right, bottom;
    LocalToScreen(x, y);
    GetClipRect(left, top, right, bottom);
    if (x < left || y < top || x + wide > right || y + tall > bottom) return;
    surface()->DrawFlushText();
    gameuimatsurface->Begin3DPaint(0, 0, wide, tall);
    gameuimodelpreview->Render(wide, tall, m_Yaw, m_AnimationTime);
    gameuimatsurface->End3DPaint();
}

void CPlayerModelPreviewPanel::OnMousePressed(MouseCode code)
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
    if (code != MOUSE_LEFT) { BaseClass::OnMousePressed(code); return; }
    m_Dragging = true;
    int mouseY;
    input()->GetCursorPos(m_LastMouseX, mouseY);
    m_LastInteractionTime = system()->GetCurrentTime();
    input()->SetMouseCapture(GetVPanel());
}

void CPlayerModelPreviewPanel::OnMouseDoublePressed(MouseCode code)
{
    if (code == MOUSE_LEFT) { OnMousePressed(MOUSE_RIGHT); return; }
    BaseClass::OnMouseDoublePressed(code);
}

void CPlayerModelPreviewPanel::OnMouseReleased(MouseCode code)
{
    if (code != MOUSE_LEFT) { BaseClass::OnMouseReleased(code); return; }
    m_Dragging = false;
    m_LastInteractionTime = system()->GetCurrentTime();
    input()->SetMouseCapture(NULL);
}

void CPlayerModelPreviewPanel::OnCursorMoved(int x, int y)
{
    if (!m_Dragging) { BaseClass::OnCursorMoved(x,y); return; }
    int mouseX, mouseY;
    input()->GetCursorPos(mouseX, mouseY);
    m_Yaw = HL2ModernModelPreview::WrapYaw(m_Yaw + (mouseX - m_LastMouseX) * 0.6f);
    m_LastMouseX = mouseX;
    m_LastInteractionTime = system()->GetCurrentTime();
    Repaint();
}

void CPlayerModelPreviewPanel::OnMouseCaptureLost()
{
    m_Dragging = false;
    m_LastInteractionTime = system()->GetCurrentTime();
    BaseClass::OnMouseCaptureLost();
}
