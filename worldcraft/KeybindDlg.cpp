//========= GammaSource: Hammer++ keybind editor ============//
//
// See KeybindDlg.h.
//
//=============================================================================//

#include "stdafx.h"
#include "worldcraft.h"
#include "MainFrm.h"
#include "ChildFrm.h"
#include "KeybindDlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static HACCEL s_hGlobalTable = NULL;	// tables built from saved keybinds (owned here)
static HACCEL s_hMapTable = NULL;

static const char *KEYBIND_SECTION = "Keybinds";


//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------
static void AccelsFromTable( HACCEL hTable, CUtlVector<ACCEL> &out )
{
	out.RemoveAll();
	if ( !hTable )
		return;
	int n = CopyAcceleratorTable( hTable, NULL, 0 );
	if ( n <= 0 )
		return;
	out.SetCount( n );
	CopyAcceleratorTable( hTable, out.Base(), n );
}

static HACCEL DefaultTable( int nResource )
{
	HINSTANCE hInst = AfxFindResourceHandle( MAKEINTRESOURCE( nResource ), RT_ACCELERATOR );
	return ::LoadAccelerators( hInst, MAKEINTRESOURCE( nResource ) );
}

static CString KeyName( const ACCEL &a )
{
	CString str;
	if ( a.fVirt & FCONTROL ) str += "Ctrl+";
	if ( a.fVirt & FSHIFT ) str += "Shift+";
	if ( a.fVirt & FALT ) str += "Alt+";

	if ( !( a.fVirt & FVIRTKEY ) )
	{
		str += (char)a.key;
		return str;
	}

	if ( ( a.key >= '0' && a.key <= '9' ) || ( a.key >= 'A' && a.key <= 'Z' ) )
	{
		str += (char)a.key;
		return str;
	}

	UINT nScan = MapVirtualKey( a.key, MAPVK_VK_TO_VSC );
	switch ( a.key )
	{
		// these share scan codes with the number pad, so ask for the extended key
		case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
		case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_DIVIDE:
			nScan |= 0x100;
			break;
	}
	char szKey[64];
	if ( GetKeyNameText( (LONG)( nScan << 16 ), szKey, sizeof( szKey ) ) > 0 )
	{
		str += szKey;
	}
	else
	{
		CString strCode;
		strCode.Format( "Key %d", a.key );
		str += strCode;
	}
	return str;
}

static bool SameKey( const ACCEL &a, const ACCEL &b )
{
	const BYTE mask = FVIRTKEY | FSHIFT | FCONTROL | FALT;
	return a.key == b.key && ( a.fVirt & mask ) == ( b.fVirt & mask );
}

// Collects every command on a menu, as "Menu > Item".
static void CollectMenu( HMENU hMenu, const CString &strPrefix, CUtlVector<CKeybindDlg::Command_t> &out )
{
	int n = GetMenuItemCount( hMenu );
	for ( int i = 0; i < n; i++ )
	{
		char szText[256] = "";
		GetMenuString( hMenu, i, szText, sizeof( szText ), MF_BYPOSITION );
		CString strText = szText;
		int nTab = strText.Find( '\t' );
		if ( nTab >= 0 )
			strText = strText.Left( nTab );
		strText.Remove( '&' );

		HMENU hSub = GetSubMenu( hMenu, i );
		if ( hSub )
		{
			CollectMenu( hSub, strPrefix + strText + " > ", out );
			continue;
		}

		UINT nID = GetMenuItemID( hMenu, i );
		if ( nID == 0 || nID == (UINT)-1 || strText.IsEmpty() )
			continue;

		bool bFound = false;
		for ( int j = 0; j < out.Count(); j++ )
		{
			if ( out[j].wID == nID )
			{
				bFound = true;
				break;
			}
		}
		if ( bFound )
			continue;

		int k = out.AddToTail();
		out[k].wID = (WORD)nID;
		out[k].strName = strPrefix + strText;
		out[k].bGlobal = false;
	}
}

static CString CommandNameFromStrings( WORD wID )
{
	CString str;
	if ( str.LoadString( wID ) && !str.IsEmpty() )
	{
		int nLine = str.Find( '\n' );
		if ( nLine >= 0 && nLine + 1 < str.GetLength() )
			str = str.Mid( nLine + 1 );
		return str;
	}
	str.Format( "Command %d", wID );
	return str;
}

// Rewrites the shortcut part ("\tCtrl+S") of every menu item from the given tables.
static void UpdateMenuShortcuts( HMENU hMenu, const CUtlVector<ACCEL> &keys )
{
	if ( !hMenu )
		return;
	int n = GetMenuItemCount( hMenu );
	for ( int i = 0; i < n; i++ )
	{
		HMENU hSub = GetSubMenu( hMenu, i );
		if ( hSub )
		{
			UpdateMenuShortcuts( hSub, keys );
			continue;
		}
		UINT nID = GetMenuItemID( hMenu, i );
		if ( nID == 0 || nID == (UINT)-1 )
			continue;

		char szText[256] = "";
		GetMenuString( hMenu, i, szText, sizeof( szText ), MF_BYPOSITION );
		CString strText = szText;
		if ( strText.IsEmpty() )
			continue;
		int nTab = strText.Find( '\t' );
		if ( nTab >= 0 )
			strText = strText.Left( nTab );

		for ( int k = 0; k < keys.Count(); k++ )
		{
			if ( keys[k].cmd == nID )
			{
				strText += "\t" + KeyName( keys[k] );
				break;
			}
		}

		MENUITEMINFO mii = { sizeof( mii ) };
		mii.fMask = MIIM_STRING;
		mii.dwTypeData = (LPSTR)(LPCSTR)strText;
		SetMenuItemInfo( hMenu, i, TRUE, &mii );
	}
}

static void ApplyTables( HACCEL hGlobal, HACCEL hMap )
{
	CMainFrame *pMain = GetMainWnd();
	CWorldcraft *pApp = APP();

	if ( pMain && hGlobal )
		pMain->m_hAccelTable = hGlobal;
	if ( hMap )
	{
		if ( pApp->pMapDocTemplate )
			pApp->pMapDocTemplate->m_hAccelTable = hMap;
		if ( pApp->pManifestDocTemplate )
			pApp->pManifestDocTemplate->m_hAccelTable = hMap;

		// map windows load their own copy of the table when they are created
		if ( pMain && pMain->m_hWndMDIClient )
		{
			for ( HWND hChild = ::GetWindow( pMain->m_hWndMDIClient, GW_CHILD ); hChild; hChild = ::GetWindow( hChild, GW_HWNDNEXT ) )
			{
				CFrameWnd *pFrame = dynamic_cast< CFrameWnd * >( CWnd::FromHandlePermanent( hChild ) );
				if ( pFrame )
					pFrame->m_hAccelTable = hMap;
			}
		}
	}

	// menus show the shortcuts in use
	CUtlVector<ACCEL> keys, mapKeys;
	AccelsFromTable( hMap, mapKeys );
	AccelsFromTable( hGlobal, keys );
	keys.AddVectorToTail( mapKeys );
	if ( pMain )
		UpdateMenuShortcuts( pMain->m_hMenuDefault, keys );
	if ( pApp->pMapDocTemplate )
		UpdateMenuShortcuts( pApp->pMapDocTemplate->m_hMenuShared, keys );
	if ( pApp->pManifestDocTemplate && pApp->pManifestDocTemplate->m_hMenuShared != ( pApp->pMapDocTemplate ? pApp->pMapDocTemplate->m_hMenuShared : NULL ) )
		UpdateMenuShortcuts( pApp->pManifestDocTemplate->m_hMenuShared, keys );
	if ( pMain )
		pMain->DrawMenuBar();
}

static HACCEL LoadSavedTable( const char *pszName )
{
	BYTE *pData = NULL;
	UINT nBytes = 0;
	if ( !AfxGetApp()->GetProfileBinary( KEYBIND_SECTION, pszName, &pData, &nBytes ) || !pData )
		return NULL;

	HACCEL hTable = NULL;
	int nKeys = nBytes / sizeof( ACCEL );
	if ( nKeys > 0 && ( nBytes % sizeof( ACCEL ) ) == 0 )
		hTable = CreateAcceleratorTable( (ACCEL *)pData, nKeys );
	delete [] pData;
	return hTable;
}

void Keybinds_ApplySaved()
{
	HACCEL hGlobal = LoadSavedTable( "Global" );
	HACCEL hMap = LoadSavedTable( "Map" );
	if ( !hGlobal && !hMap )
		return;

	ApplyTables( hGlobal ? hGlobal : GetMainWnd()->m_hAccelTable, hMap ? hMap : APP()->pMapDocTemplate->m_hAccelTable );

	// the old tables may still be referenced by frames until they are replaced, so only
	// tables this file created earlier are destroyed
	if ( hGlobal )
	{
		if ( s_hGlobalTable )
			DestroyAcceleratorTable( s_hGlobalTable );
		s_hGlobalTable = hGlobal;
	}
	if ( hMap )
	{
		if ( s_hMapTable )
			DestroyAcceleratorTable( s_hMapTable );
		s_hMapTable = hMap;
	}
}

HACCEL Keybinds_GetMapTable()
{
	return s_hMapTable;
}


//-----------------------------------------------------------------------------
// The dialog
//-----------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CKeybindDlg, CDialog)
	ON_BN_CLICKED(IDC_KEYBIND_ASSIGN, OnAssign)
	ON_BN_CLICKED(IDC_KEYBIND_REMOVE, OnRemove)
	ON_BN_CLICKED(IDC_KEYBIND_RESET, OnResetAll)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_KEYBIND_LIST, OnItemChanged)
END_MESSAGE_MAP()


CKeybindDlg::CKeybindDlg( CWnd *pParent ) : CDialog( CKeybindDlg::IDD, pParent )
{
}


void CKeybindDlg::LoadCommands( HACCEL hGlobal, HACCEL hMap )
{
	m_Commands.RemoveAll();

	// every menu command, in menu order
	HINSTANCE hInst = AfxFindResourceHandle( MAKEINTRESOURCE( IDR_MAPDOC ), RT_MENU );
	HMENU hMenu = ::LoadMenu( hInst, MAKEINTRESOURCE( IDR_MAPDOC ) );
	if ( hMenu )
	{
		CollectMenu( hMenu, "", m_Commands );
		DestroyMenu( hMenu );
	}

	// add the keys, and commands that only have a key
	for ( int nTable = 0; nTable < 2; nTable++ )
	{
		CUtlVector<ACCEL> keys;
		AccelsFromTable( nTable == 0 ? hGlobal : hMap, keys );
		for ( int i = 0; i < keys.Count(); i++ )
		{
			int nCmd = -1;
			for ( int j = 0; j < m_Commands.Count(); j++ )
			{
				if ( m_Commands[j].wID == keys[i].cmd )
				{
					nCmd = j;
					break;
				}
			}
			if ( nCmd < 0 )
			{
				nCmd = m_Commands.AddToTail();
				m_Commands[nCmd].wID = keys[i].cmd;
				m_Commands[nCmd].strName = CommandNameFromStrings( keys[i].cmd );
				m_Commands[nCmd].bGlobal = ( nTable == 0 );
			}
			else if ( nTable == 0 && m_Commands[nCmd].Keys.Count() == 0 )
			{
				m_Commands[nCmd].bGlobal = true;
			}

			bool bDuplicate = false;
			for ( int k = 0; k < m_Commands[nCmd].Keys.Count(); k++ )
			{
				if ( SameKey( m_Commands[nCmd].Keys[k], keys[i] ) )
					bDuplicate = true;
			}
			if ( !bDuplicate )
				m_Commands[nCmd].Keys.AddToTail( keys[i] );
		}
	}
}


void CKeybindDlg::FillList()
{
	int nSel = SelectedCommand();
	m_List.DeleteAllItems();
	for ( int i = 0; i < m_Commands.Count(); i++ )
	{
		CString strKeys;
		for ( int k = 0; k < m_Commands[i].Keys.Count(); k++ )
		{
			if ( k )
				strKeys += ", ";
			strKeys += KeyName( m_Commands[i].Keys[k] );
		}
		int nItem = m_List.InsertItem( i, m_Commands[i].strName );
		m_List.SetItemText( nItem, 1, strKeys );
		m_List.SetItemText( nItem, 2, m_Commands[i].bGlobal ? "Always" : "Map open" );
		m_List.SetItemData( nItem, i );
	}
	if ( nSel >= 0 )
	{
		m_List.SetItemState( nSel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
		m_List.EnsureVisible( nSel, FALSE );
	}
}


int CKeybindDlg::SelectedCommand()
{
	if ( !m_List.GetSafeHwnd() )
		return -1;
	POSITION pos = m_List.GetFirstSelectedItemPosition();
	if ( !pos )
		return -1;
	return (int)m_List.GetItemData( m_List.GetNextSelectedItem( pos ) );
}


BOOL CKeybindDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_List.SubclassDlgItem( IDC_KEYBIND_LIST, this );
	m_HotKey.SubclassDlgItem( IDC_KEYBIND_HOTKEY, this );

	m_List.SetExtendedStyle( LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES );
	m_List.InsertColumn( 0, "Command", LVCFMT_LEFT, 300 );
	m_List.InsertColumn( 1, "Shortcut", LVCFMT_LEFT, 150 );
	m_List.InsertColumn( 2, "Works when", LVCFMT_LEFT, 80 );

	// keys typed into the box are shortcuts, not dialog navigation
	m_HotKey.SetRules( 0, 0 );

	LoadCommands( GetMainWnd()->m_hAccelTable, APP()->pMapDocTemplate->m_hAccelTable );
	FillList();
	return TRUE;
}


void CKeybindDlg::OnItemChanged( NMHDR *pNMHDR, LRESULT *pResult )
{
	int nCmd = SelectedCommand();
	if ( nCmd >= 0 && m_Commands[nCmd].Keys.Count() )
	{
		const ACCEL &a = m_Commands[nCmd].Keys[0];
		WORD wMods = 0;
		if ( a.fVirt & FSHIFT ) wMods |= HOTKEYF_SHIFT;
		if ( a.fVirt & FCONTROL ) wMods |= HOTKEYF_CONTROL;
		if ( a.fVirt & FALT ) wMods |= HOTKEYF_ALT;
		m_HotKey.SetHotKey( a.key, wMods );
	}
	else
	{
		m_HotKey.SetHotKey( 0, 0 );
	}
	*pResult = 0;
}


void CKeybindDlg::OnAssign()
{
	int nCmd = SelectedCommand();
	WORD wVK = 0, wMods = 0;
	m_HotKey.GetHotKey( wVK, wMods );
	if ( nCmd < 0 || !wVK )
	{
		SetDlgItemText( IDC_KEYBIND_STATUS, "Pick a command, then press the new shortcut in the box." );
		return;
	}

	ACCEL a;
	a.fVirt = FVIRTKEY | FNOINVERT;
	if ( wMods & HOTKEYF_SHIFT ) a.fVirt |= FSHIFT;
	if ( wMods & HOTKEYF_CONTROL ) a.fVirt |= FCONTROL;
	if ( wMods & HOTKEYF_ALT ) a.fVirt |= FALT;
	a.key = wVK;
	a.cmd = m_Commands[nCmd].wID;

	// a key does one thing: take it away from any other command
	CString strStatus;
	for ( int i = 0; i < m_Commands.Count(); i++ )
	{
		for ( int k = m_Commands[i].Keys.Count() - 1; k >= 0; k-- )
		{
			if ( SameKey( m_Commands[i].Keys[k], a ) )
			{
				if ( i != nCmd )
					strStatus.Format( "%s was taken from \"%s\".", (LPCSTR)KeyName( a ), (LPCSTR)m_Commands[i].strName );
				m_Commands[i].Keys.Remove( k );
			}
		}
	}

	m_Commands[nCmd].Keys.RemoveAll();
	m_Commands[nCmd].Keys.AddToTail( a );
	SetDlgItemText( IDC_KEYBIND_STATUS, strStatus );
	FillList();
}


void CKeybindDlg::OnRemove()
{
	int nCmd = SelectedCommand();
	if ( nCmd < 0 )
		return;
	m_Commands[nCmd].Keys.RemoveAll();
	m_HotKey.SetHotKey( 0, 0 );
	FillList();
}


void CKeybindDlg::OnResetAll()
{
	LoadCommands( DefaultTable( IDR_MAINFRAME ), DefaultTable( IDR_MAPDOC ) );
	SetDlgItemText( IDC_KEYBIND_STATUS, "Default shortcuts restored. Press OK to keep them." );
	FillList();
}


void CKeybindDlg::OnOK()
{
	CUtlVector<ACCEL> global, map;
	for ( int i = 0; i < m_Commands.Count(); i++ )
	{
		for ( int k = 0; k < m_Commands[i].Keys.Count(); k++ )
		{
			( m_Commands[i].bGlobal ? global : map ).AddToTail( m_Commands[i].Keys[k] );
		}
	}

	AfxGetApp()->WriteProfileBinary( KEYBIND_SECTION, "Global", (LPBYTE)global.Base(), global.Count() * sizeof( ACCEL ) );
	AfxGetApp()->WriteProfileBinary( KEYBIND_SECTION, "Map", (LPBYTE)map.Base(), map.Count() * sizeof( ACCEL ) );
	Keybinds_ApplySaved();

	CDialog::OnOK();
}
