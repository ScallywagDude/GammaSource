//========= GammaSource: Hammer++ keybind editor ============//
//
// Tools > Keyboard Shortcuts: lists every menu command with its shortcut and lets the user
// assign or remove keys. The edited tables replace the editor's accelerator tables (the
// main frame's "global" one and the map document's one) and are saved under
// HKCU\Software\Valve\Hammer\Keybinds, then applied again at startup.
//
//=============================================================================//

#ifndef KEYBINDDLG_H
#define KEYBINDDLG_H
#pragma once

#include "resource.h"
#include "utlvector.h"

// Applies saved keybinds (if any) to the main frame, the map document templates and
// any open map windows. Called at startup and after the dialog's OK.
void Keybinds_ApplySaved();

// The customised map accelerator table, or NULL if the defaults are in use.
HACCEL Keybinds_GetMapTable();


class CKeybindDlg : public CDialog
{
public:
	CKeybindDlg( CWnd *pParent = NULL );

	enum { IDD = IDD_KEYBINDS };

	struct Command_t
	{
		WORD wID;
		CString strName;
		bool bGlobal;					// lives in the main frame's table
		CUtlVector<ACCEL> Keys;
	};

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	afx_msg void OnAssign();
	afx_msg void OnRemove();
	afx_msg void OnResetAll();
	afx_msg void OnItemChanged( NMHDR *pNMHDR, LRESULT *pResult );
	DECLARE_MESSAGE_MAP()

private:
	void LoadCommands( HACCEL hGlobal, HACCEL hMap );
	void FillList();
	int SelectedCommand();

	CListCtrl m_List;
	CHotKeyCtrl m_HotKey;
	CUtlVector<Command_t> m_Commands;
};

#endif // KEYBINDDLG_H
