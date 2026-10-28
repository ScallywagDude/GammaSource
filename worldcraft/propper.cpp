//========= GammaSource: Hammer++ Propper ============//
//
// Tools > Compile Selection to Model: turns the selected brushes into a static prop. The
// brush faces are written as an SMD (textures and UVs as in the editor; tool textures are
// left out of the visible model), each brush also becomes a convex piece of the collision
// model, and studiomdl compiles them into models/<name>.mdl in the game directory. The
// brushes are then replaced by a prop_static at the bottom middle of the selection.
// Undo step: "Compile to Model". The sources stay in <game>/modelsrc/worldcraft/<name>.
//
//=============================================================================//

#include "stdafx.h"
#include "worldcraft.h"
#include "MapDoc.h"
#include "MainFrm.h"
#include "MapEntity.h"
#include "MapSolid.h"
#include "MapFace.h"
#include "MapWorld.h"
#include "History.h"
#include "GlobalFunctions.h"
#include "StatusBarIDs.h"
#include "Selection.h"
#include "StrDlg.h"
#include "GameConfig.h"
#include "Options.h"
#include <shlobj.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static void CollectSolids( CMapClass *pObject, CUtlVector<CMapSolid *> &out )
{
	CMapSolid *pSolid = dynamic_cast< CMapSolid * >( pObject );
	if ( pSolid )
	{
		out.AddToTail( pSolid );
		return;
	}
	const CMapObjectList *pChildren = pObject->GetChildren();
	for ( int i = 0; i < pChildren->Count(); i++ )
	{
		CollectSolids( pChildren->Element( i ), out );
	}
}

static bool IsToolTexture( const char *pszTexture )
{
	return V_strnicmp( pszTexture, "tools/", 6 ) == 0 || V_strnicmp( pszTexture, "tools\\", 6 ) == 0;
}

// Writes the faces as SMD triangles, positions relative to vOrigin. bVisual leaves out tool
// textures and uses the real materials; the collision model uses one material for everything.
static void WriteSMD( FILE *fp, const CUtlVector<CMapSolid *> &solids, const Vector &vOrigin, bool bVisual )
{
	fprintf( fp, "version 1\nnodes\n0 \"root\" -1\nend\nskeleton\ntime 0\n0 0 0 0 0 0 0\nend\ntriangles\n" );
	for ( int s = 0; s < solids.Count(); s++ )
	{
		CMapSolid *pSolid = solids[s];
		for ( int f = 0; f < pSolid->GetFaceCount(); f++ )
		{
			CMapFace *pFace = pSolid->GetFace( f );
			if ( pFace->nPoints < 3 )
				continue;

			char szTexture[MAX_PATH];
			pFace->GetTextureName( szTexture );
			if ( bVisual && IsToolTexture( szTexture ) )
				continue;
			V_strlower( szTexture );
			V_FixSlashes( szTexture, '/' );

			pFace->CalcTextureCoords();
			const Vector &vNormal = pFace->plane.normal;

			// fan triangles, counter-clockwise seen from outside
			for ( int i = 1; i + 1 < pFace->nPoints; i++ )
			{
				int idx[3] = { 0, i, i + 1 };
				Vector vCross = CrossProduct( pFace->Points[idx[1]] - pFace->Points[idx[0]], pFace->Points[idx[2]] - pFace->Points[idx[0]] );
				if ( DotProduct( vCross, vNormal ) < 0 )
				{
					V_swap( idx[1], idx[2] );
				}

				fprintf( fp, "%s\n", bVisual ? szTexture : "phy" );
				for ( int v = 0; v < 3; v++ )
				{
					Vector p = pFace->Points[idx[v]] - vOrigin;
					Vector2D uv;
					pFace->GetTexCoord( uv, idx[v] );
					fprintf( fp, "0 %f %f %f %f %f %f %f %f\n", p.x, p.y, p.z, vNormal.x, vNormal.y, vNormal.z, uv.x, 1.0f - uv.y );
				}
			}
		}
	}
	fprintf( fp, "end\n" );
}

bool Propper_FindStudiomdl( char *pszPath, int nSize )
{
	// next to the editor, then the remembered location, then ask
	char szDir[MAX_PATH];
	APP()->GetDirectory( DIR_PROGRAM, szDir );
	V_ComposeFileName( szDir, "studiomdl.exe", pszPath, nSize );
	if ( GetFileAttributes( pszPath ) != INVALID_FILE_ATTRIBUTES )
		return true;

	CString strSaved = APP()->GetProfileString( "General", "StudiomdlPath", "" );
	if ( !strSaved.IsEmpty() && GetFileAttributes( strSaved ) != INVALID_FILE_ATTRIBUTES )
	{
		V_strncpy( pszPath, strSaved, nSize );
		return true;
	}

	CFileDialog dlg( TRUE, "exe", "studiomdl.exe", OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR, "studiomdl.exe|studiomdl.exe||", GetMainWnd() );
	dlg.m_ofn.lpstrTitle = "Where is studiomdl.exe? (for example Half-Life 2\\bin)";
	if ( dlg.DoModal() != IDOK )
		return false;
	V_strncpy( pszPath, dlg.GetPathName(), nSize );
	APP()->WriteProfileString( "General", "StudiomdlPath", pszPath );
	return true;
}

bool Propper_RunStudiomdl( const char *pszStudiomdl, const char *pszGameDir, const char *pszQC, const char *pszLog )
{
	char szCmd[MAX_PATH * 4];
	V_snprintf( szCmd, sizeof( szCmd ), "\"%s\" -nop4 -game \"%s\" \"%s\"", pszStudiomdl, pszGameDir, pszQC );

	char szWorkDir[MAX_PATH];
	V_ExtractFilePath( pszStudiomdl, szWorkDir, sizeof( szWorkDir ) );

	SECURITY_ATTRIBUTES sa = { sizeof( sa ), NULL, TRUE };
	HANDLE hLog = CreateFile( pszLog, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

	STARTUPINFO si = { sizeof( si ) };
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = hLog;
	si.hStdError = hLog;
	si.hStdInput = GetStdHandle( STD_INPUT_HANDLE );
	PROCESS_INFORMATION pi;
	BOOL bStarted = CreateProcess( NULL, szCmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, szWorkDir, &si, &pi );
	if ( hLog != INVALID_HANDLE_VALUE )
		CloseHandle( hLog );
	if ( !bStarted )
		return false;

	CWaitCursor wait;
	WaitForSingleObject( pi.hProcess, 5 * 60 * 1000 );
	DWORD dwExit = 1;
	GetExitCodeProcess( pi.hProcess, &dwExit );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
	return dwExit == 0;
}

void PropperCompileSelection( CMapDoc *pDoc )
{
	const CMapObjectList *pSelList = pDoc->GetSelection()->GetList();
	CUtlVector<CMapSolid *> solids;
	for ( int i = 0; i < pSelList->Count(); i++ )
	{
		CollectSolids( pSelList->Element( i ), solids );
	}
	if ( !solids.Count() )
	{
		AfxMessageBox( "Select the brushes to turn into a model.", MB_ICONINFORMATION | MB_OK );
		return;
	}

	// a name for the model
	static int s_nCount = 0;
	char szDefault[MAX_PATH];
	CString strMap = pDoc->GetPathName();
	char szMapBase[MAX_PATH] = "map";
	if ( !strMap.IsEmpty() )
		V_FileBase( strMap, szMapBase, sizeof( szMapBase ) );
	V_snprintf( szDefault, sizeof( szDefault ), "props_worldcraft/%s_prop%d", szMapBase, ++s_nCount );
	CStrDlg dlg( 0, szDefault, "Model name (under models/, without .mdl):", "Compile Selection to Model" );
	if ( dlg.DoModal() != IDOK )
		return;
	char szName[MAX_PATH];
	V_strncpy( szName, dlg.m_string, sizeof( szName ) );
	V_FixSlashes( szName, '/' );
	V_StripExtension( szName, szName, sizeof( szName ) );
	if ( !V_strnicmp( szName, "models/", 7 ) )
		memmove( szName, szName + 7, strlen( szName + 7 ) + 1 );
	if ( !szName[0] )
		return;

	char szStudiomdl[MAX_PATH];
	if ( !Propper_FindStudiomdl( szStudiomdl, sizeof( szStudiomdl ) ) )
		return;

	const char *pszGameDir = g_pGameConfig->m_szModDir;

	// the model's origin: bottom middle of the selection, on the grid
	Vector vMins, vMaxs;
	pDoc->GetSelection()->GetBounds( vMins, vMaxs );
	Vector vOrigin( ( vMins.x + vMaxs.x ) * 0.5f, ( vMins.y + vMaxs.y ) * 0.5f, vMins.z );
	pDoc->Snap( vOrigin );
	vOrigin.z = vMins.z;

	// write the sources
	char szSrcDir[MAX_PATH], szTmp[MAX_PATH];
	V_ComposeFileName( pszGameDir, "modelsrc/worldcraft", szTmp, sizeof( szTmp ) );
	V_ComposeFileName( szTmp, szName, szSrcDir, sizeof( szSrcDir ) );
	V_FixSlashes( szSrcDir );
	SHCreateDirectoryEx( NULL, szSrcDir, NULL );

	char szBase[MAX_PATH];
	V_FileBase( szName, szBase, sizeof( szBase ) );
	char szRef[MAX_PATH], szPhys[MAX_PATH], szQC[MAX_PATH], szLog[MAX_PATH], szFile[MAX_PATH];
	V_snprintf( szFile, sizeof( szFile ), "%s_ref.smd", szBase ); V_ComposeFileName( szSrcDir, szFile, szRef, sizeof( szRef ) );
	V_snprintf( szFile, sizeof( szFile ), "%s_phys.smd", szBase ); V_ComposeFileName( szSrcDir, szFile, szPhys, sizeof( szPhys ) );
	V_snprintf( szFile, sizeof( szFile ), "%s.qc", szBase ); V_ComposeFileName( szSrcDir, szFile, szQC, sizeof( szQC ) );
	V_snprintf( szFile, sizeof( szFile ), "%s_studiomdl.log", szBase ); V_ComposeFileName( szSrcDir, szFile, szLog, sizeof( szLog ) );

	FILE *fp = fopen( szRef, "wt" );
	if ( !fp )
	{
		AfxMessageBox( "Could not write the model sources." );
		return;
	}
	WriteSMD( fp, solids, vOrigin, true );
	fclose( fp );

	fp = fopen( szPhys, "wt" );
	if ( fp )
	{
		WriteSMD( fp, solids, vOrigin, false );
		fclose( fp );
	}

	fp = fopen( szQC, "wt" );
	if ( !fp )
	{
		AfxMessageBox( "Could not write the model sources." );
		return;
	}
	fprintf( fp, "// written by Worldcraft (Compile Selection to Model)\n" );
	fprintf( fp, "$modelname \"%s.mdl\"\n", szName );
	fprintf( fp, "$staticprop\n" );
	fprintf( fp, "$origin 0 0 0 -90\n" );	// cancel studiomdl's default 90 degree turn, so the model faces like the brushes
	fprintf( fp, "$surfaceprop \"default\"\n" );
	fprintf( fp, "$cdmaterials \"\"\n" );
	fprintf( fp, "$body body \"%s_ref.smd\"\n", szBase );
	fprintf( fp, "$sequence idle \"%s_ref.smd\"\n", szBase );
	fprintf( fp, "$collisionmodel \"%s_phys.smd\"\n{\n\t$concave\n\t$maxconvexpieces 1024\n\t$mass 100\n}\n", szBase );
	fclose( fp );

	SetStatusText( SBI_PROMPT, "Compiling the model with studiomdl..." );
	char szModel[MAX_PATH], szModelRel[MAX_PATH];
	V_snprintf( szModelRel, sizeof( szModelRel ), "models/%s.mdl", szName );
	V_ComposeFileName( pszGameDir, szModelRel, szModel, sizeof( szModel ) );
	V_FixSlashes( szModel );
	DeleteFile( szModel );

	if ( !Propper_RunStudiomdl( szStudiomdl, pszGameDir, szQC, szLog ) || GetFileAttributes( szModel ) == INVALID_FILE_ATTRIBUTES )
	{
		char szMsg[MAX_PATH * 2];
		V_snprintf( szMsg, sizeof( szMsg ), "studiomdl could not compile the model. Its output is in\n%s", szLog );
		AfxMessageBox( szMsg, MB_ICONEXCLAMATION | MB_OK );
		SetStatusText( SBI_PROMPT, "" );
		return;
	}

	// swap the brushes for the prop
	CMapObjectList Objects;
	Objects.AddVectorToTail( *pSelList );
	GetHistory()->MarkUndoPosition( pSelList, "Compile to Model" );

	CMapEntity *pEntity = new CMapEntity;
	pEntity->SetPlaceholder( TRUE );
	pEntity->SetOrigin( vOrigin );
	pEntity->SetClass( "prop_static" );
	pEntity->SetKeyValue( "angles", "0 0 0" );
	pEntity->SetKeyValue( "model", szModelRel );
	pDoc->AddObjectToWorld( pEntity );
	GetHistory()->KeepNew( pEntity );

	// only what the model was built from goes: selected lights, props etc. stay
	for ( int i = 0; i < Objects.Count(); i++ )
	{
		CUtlVector<CMapSolid *> objectSolids;
		CollectSolids( Objects.Element( i ), objectSolids );
		if ( objectSolids.Count() )
			pDoc->DeleteObject( Objects.Element( i ) );
	}
	pDoc->SelectObject( pEntity, scClear | scSelect );
	pDoc->SetModifiedFlag();
	pDoc->UpdateAllViews( MAPVIEW_UPDATE_OBJECTS );

	char szStatus[MAX_PATH];
	V_snprintf( szStatus, sizeof( szStatus ), "Compiled %s from %d brush%s.", szModelRel, solids.Count(), solids.Count() == 1 ? "" : "es" );
	SetStatusText( SBI_PROMPT, szStatus );
}
