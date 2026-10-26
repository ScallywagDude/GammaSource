#ifndef PLAYER_MODEL_PREVIEW_PANEL_H
#define PLAYER_MODEL_PREVIEW_PANEL_H
#ifdef _WIN32
#pragma once
#endif
#include <vgui_controls/Panel.h>

class CPlayerModelPreviewPanel : public vgui::Panel
{
    typedef vgui::Panel BaseClass;
public:
    CPlayerModelPreviewPanel(vgui::Panel *parent, const char *name);
    virtual ~CPlayerModelPreviewPanel();
    bool SetModel(const char *path);
    void ClearModel();
    const char *GetStatusText();
protected:
    virtual void Paint();
    virtual void OnThink();
    virtual void OnMousePressed(vgui::MouseCode code);
    virtual void OnMouseDoublePressed(vgui::MouseCode code);
    virtual void OnMouseReleased(vgui::MouseCode code);
    virtual void OnCursorMoved(int x, int y);
    virtual void OnMouseCaptureLost();
private:
    float m_Yaw;
    float m_AnimationTime;
    double m_LastFrameTime;
    double m_LastInteractionTime;
    int m_LastMouseX;
    bool m_Dragging;
    bool m_Paused; // HL2MODERN_PREVIEW_CONTROLS_V19
    bool m_HasModel;
};
#endif
