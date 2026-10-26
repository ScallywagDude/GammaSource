//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: GammaSource: the 2003 options "Advanced" tab (content lock)
//
// $NoKeywords: $
//=============================================================================

#include "OptionsSubAdvanced.h"

#include <vgui_controls/CheckButton.h>
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include "ContentControlDialog.h"

using namespace vgui;
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubAdvanced::COptionsSubAdvanced(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
	m_pContentCheckButton = new vgui::CheckButton( this, "ContentlockButton", "#GameUI_ContentLock" );
	m_pContentCheckButton->SetCommand( "ContentControl" );

	LoadControlSettings("Resource\\OptionsSubAdvanced.res");

    // set up the content control dialog
    m_pContentControlDialog = new CContentControlDialog(this);
    m_pContentControlDialog->AddActionSignalTarget(this);

    int x, y, ww, wt, wide, tall;
    surface()->GetWorkspaceBounds( x, y, ww, wt );
    m_pContentControlDialog->GetSize(wide, tall);
    if ( ww < wide || wt < tall )
    {
        x = y = 0;
        surface()->GetScreenSize( ww, wt );
    }

    // Center it, keeping requested size
    m_pContentControlDialog->SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));

    m_pContentCheckButton->SetSelected( m_pContentControlDialog->IsPasswordEnabled() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnCommand( const char *command )
{
	if ( !stricmp( command, "ContentControl" ) )
	{
        // we're trying to turn it off
        if (!m_pContentCheckButton->IsSelected())
        {
            // if there's no password originally, we can just reset it and move on
            if (!m_pContentControlDialog->IsPasswordEnabled())
            {
                m_pContentControlDialog->ResetPassword();
                return;
            }
        }
        OnOpenContentControlDialog();
		return;
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubAdvanced::~COptionsSubAdvanced()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnResetData()
{
    m_pContentControlDialog->ResetPassword();
    m_pContentCheckButton->SetSelected( m_pContentControlDialog->IsPasswordEnabled() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnApplyChanges()
{
    m_pContentControlDialog->ApplyPassword();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnContentControlClose()
{
    m_pContentCheckButton->SetSelected(m_pContentControlDialog->IsPasswordEnabledInDialog());

	if (m_pContentControlDialog->IsPasswordEnabled() != m_pContentControlDialog->IsPasswordEnabledInDialog())
    {
        PostActionSignal(new KeyValues("ApplyButtonEnable"));
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnOpenContentControlDialog()
{
	m_pContentControlDialog->Activate();
}
