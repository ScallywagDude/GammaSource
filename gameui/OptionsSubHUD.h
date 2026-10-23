// HUD options share the client's archived console variables. Changes apply on OK/Apply.
#ifndef OPTIONS_SUB_HUD_H
#define OPTIONS_SUB_HUD_H
#include "EngineInterface.h"
#include "ConVar.h"
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <KeyValues.h>
#include "../modernize/hud_palette.h"
#include <stdio.h>
class COptionsSubHUD : public vgui::PropertyPage
{
    typedef vgui::PropertyPage BaseClass;
    DECLARE_PANELMAP();
public:
    COptionsSubHUD(vgui::Panel *parent) : BaseClass(parent,"HUD")
    {
        m_Custom=new vgui::CheckButton(this,"CustomHUDColor","Use custom HUD color");
        m_Custom->SetBounds(20,18,350,24); m_Custom->AddActionSignalTarget(this);
        const char *names[]={"Red","Green","Blue"};
        for(int i=0;i<3;++i)
        {
            vgui::Label *label=new vgui::Label(this,names[i],names[i]);
            label->SetBounds(20,58+i*40,60,24);
            m_RGB[i]=new vgui::Slider(this,names[i]);
            m_RGB[i]->SetBounds(84,58+i*40,280,28); m_RGB[i]->SetRange(0,255);
            m_RGB[i]->SetTickCaptions("0","255"); m_RGB[i]->AddActionSignalTarget(this);
            m_Value[i]=new vgui::Label(this,"Value","");
            m_Value[i]->SetBounds(374,58+i*40,60,24);
        }
        m_Preview=new vgui::Label(this,"Preview","HEALTH 100    HEV 100    Energy 100");
        m_Preview->SetBounds(20,190,440,32);
        vgui::Button *reset=new vgui::Button(this,"Defaults","Restore 2002 color");
        reset->SetCommand("HUDDefaults"); reset->AddActionSignalTarget(this);
        reset->SetBounds(20,236,190,26);
        vgui::Label *hint=new vgui::Label(this,"Hint","HUD changes are saved when you press Apply or OK.");
        hint->SetBounds(20,270,440,24);
        OnResetData();
    }
    virtual void OnResetData()
    {
        const char *names[]={"hud_color_r","hud_color_g","hud_color_b"};
        const int defaults[]={247,192,84};
        const ConVar *custom=cvar->FindVar("hud_color_custom");
        m_Custom->SetSelected(custom && custom->GetBool());
        for(int i=0;i<3;++i)
        {
            const ConVar *var=cvar->FindVar(names[i]);
            m_RGB[i]->SetValue(var?HL2ModernHud::ColorByte(var->GetInt()):defaults[i],false);
        }
        UpdatePreview();
    }
    virtual void OnApplyChanges()
    {
        const char *names[]={"hud_color_r","hud_color_g","hud_color_b"};
        for(int i=0;i<3;++i)
        {
            ConVar *var=(ConVar*)cvar->FindVar(names[i]);
            if(var) var->SetValue(HL2ModernHud::ColorByte(m_RGB[i]->GetValue()));
        }
        ConVar *custom=(ConVar*)cvar->FindVar("hud_color_custom");
        if(custom) custom->SetValue(m_Custom->IsSelected()?1:0);
    }
    virtual void OnCommand(const char *command)
    {
        if(!stricmp(command,"HUDDefaults"))
        {
            m_Custom->SetSelected(false);
            m_RGB[0]->SetValue(247,false); m_RGB[1]->SetValue(192,false); m_RGB[2]->SetValue(84,false);
            OnColorChanged();
        }
        else BaseClass::OnCommand(command);
    }
private:
    void OnColorChanged() { UpdatePreview(); PostActionSignal(new KeyValues("ApplyButtonEnable")); }
    void UpdatePreview()
    {
        int rgb[3]; const int defaults[]={247,192,84};
        for(int i=0;i<3;++i)
        {
            rgb[i]=HL2ModernHud::ColorByte(m_RGB[i]->GetValue());
            char value[8]; sprintf(value,"%d",rgb[i]); m_Value[i]->SetText(value);
            m_RGB[i]->SetEnabled(m_Custom->IsSelected());
            if(!m_Custom->IsSelected()) rgb[i]=defaults[i];
        }
        m_Preview->SetFgColor(Color(rgb[0],rgb[1],rgb[2],255));
    }
    vgui::CheckButton *m_Custom;
    vgui::Slider *m_RGB[3];
    vgui::Label *m_Preview,*m_Value[3];
};
vgui::MessageMapItem_t COptionsSubHUD::m_MessageMap[] =
{
    MAP_MESSAGE(COptionsSubHUD,"SliderMoved",OnColorChanged),
    MAP_MESSAGE(COptionsSubHUD,"CheckButtonChecked",OnColorChanged),
};
IMPLEMENT_PANELMAP(COptionsSubHUD,BaseClass);
#endif
