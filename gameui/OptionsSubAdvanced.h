//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: GammaSource: the 2003 options "Advanced" tab (content lock)
//
// $NoKeywords: $
//=============================================================================

#ifndef OPTIONS_SUB_ADVANCED_H
#define OPTIONS_SUB_ADVANCED_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

namespace vgui
{
    class CheckButton;
}

class CContentControlDialog;
//-----------------------------------------------------------------------------
// Purpose: Advanced Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubAdvanced : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubAdvanced, vgui::PropertyPage );

public:
	COptionsSubAdvanced(vgui::Panel *parent);
	~COptionsSubAdvanced();

	virtual void OnResetData();
	virtual void OnApplyChanges();

	virtual void OnCommand( const char *command );

    void OnOpenContentControlDialog();

private:
    MESSAGE_FUNC( OnContentControlClose, "ContentControlClose" );

	CContentControlDialog   *m_pContentControlDialog;
    vgui::CheckButton       *m_pContentCheckButton;
};

#endif // OPTIONS_SUB_ADVANCED_H
