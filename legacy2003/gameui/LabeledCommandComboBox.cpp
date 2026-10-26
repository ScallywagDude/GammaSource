//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "LabeledCommandComboBox.h"
#include "EngineInterface.h"
#include <KeyValues.h>
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CLabeledCommandComboBox::CLabeledCommandComboBox( vgui::Panel *parent, const char *panelName ) : vgui::ComboBox( parent, panelName, 6, false )
{
	AddActionSignalTarget(this);
	m_iCurrentSelection = -1;
	m_iStartSelection = -1;
}

CLabeledCommandComboBox::~CLabeledCommandComboBox( void )
{
}

void CLabeledCommandComboBox::DeleteAllItems()
{
	BaseClass::DeleteAllItems();
	m_Items.RemoveAll();
	m_iCurrentSelection = -1;
	m_iStartSelection = -1; // HL2MODERN_MODEL_UI_SAFETY
}

void CLabeledCommandComboBox::AddItem( char const *text, char const *engineCommand )
{
	int idx = m_Items.AddToTail();
	COMMANDITEM *item = &m_Items[ idx ];

	item->comboBoxID = BaseClass::AddItem( text, NULL );

	const char *safeText = text ? text : "";
	const char *safeCommand = engineCommand ? engineCommand : "";
	strncpy(item->name, safeText, sizeof(item->name)-1);
	item->name[sizeof(item->name)-1] = 0;

	if (safeText[0] == '#')
	{
		// need to localize the string
		wchar_t *localized = vgui::localize()->Find(text);
		if (localized)
		{
			vgui::localize()->ConvertUnicodeToANSI(localized, item->name, sizeof(item->name));
		}
	}

	strncpy(item->command, safeCommand, sizeof(item->command)-1);
	item->command[sizeof(item->command)-1] = 0;
}

void CLabeledCommandComboBox::ActivateItem(int index)
{
	if ( index >= 0 && index < m_Items.Count() )
	{
		int comboBoxID = m_Items[index].comboBoxID;
		BaseClass::ActivateItem(comboBoxID);
		m_iCurrentSelection = index;
	}
}

void CLabeledCommandComboBox::SetInitialItem(int index)
{
	if ( index >= 0 && index < m_Items.Count() )
	{
		m_iStartSelection = index;
		ActivateItem(index); // internal index, not ComboBox handle
	}
}

void CLabeledCommandComboBox::OnTextChanged( char const *text )
{
	// HL2MODERN_COMBO_INDEX_GUARD: never keep the previous command when the
	// current label is not present (for example after list reload or edits).
	m_iCurrentSelection = -1;
	int i;
	for ( i = 0; i < m_Items.Size(); i++ )
	{
		COMMANDITEM *item = &m_Items[ i ];
		if ( !stricmp( item->name, text ) )
		{
		//	engine->pfnClientCmd( item->command );
			m_iCurrentSelection = i;
			break;
		}
	}

	// HL2MODERN_COMBO_LIVE_UPDATE: notify the owner with the originating panel.
	KeyValues *changed = new KeyValues("TextChanged");
	changed->SetPtr("panel", this);
	PostMessage(GetParent(), changed);
	if (HasBeenModified())
	{
		PostActionSignal(new KeyValues("ControlModified"));
	}
//	PostMessage( GetParent()->GetVPanel(), new vgui::KeyValues( "TextChanged", "text", text ) );
}

const char *CLabeledCommandComboBox::GetActiveItemCommand()
{
	if (m_iCurrentSelection < 0 || m_iCurrentSelection >= m_Items.Count())
		return NULL;

	COMMANDITEM *item = &m_Items[ m_iCurrentSelection ];
	return item->command;
}

void CLabeledCommandComboBox::ApplyChanges()
{
	if (m_iCurrentSelection < 0 || m_iCurrentSelection >= m_Items.Count())
		return;
	if (m_Items.Size() < 1)
		return;

	assert( m_iCurrentSelection < m_Items.Size() );
	COMMANDITEM *item = &m_Items[ m_iCurrentSelection ];
	engine->ClientCmd( item->command );
	m_iStartSelection = m_iCurrentSelection;
}

bool CLabeledCommandComboBox::HasBeenModified()
{
	return m_iStartSelection != m_iCurrentSelection;
}

void CLabeledCommandComboBox::Reset()
{
	if (m_iStartSelection != -1)
	{
		ActivateItem(m_iStartSelection);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Message mapping 
//-----------------------------------------------------------------------------
vgui::MessageMapItem_t CLabeledCommandComboBox::m_MessageMap[] =
{
	MAP_MESSAGE_CONSTCHARPTR( CLabeledCommandComboBox, "TextChanged", OnTextChanged, "text" ),	// custom message
};

IMPLEMENT_PANELMAP( CLabeledCommandComboBox, BaseClass );