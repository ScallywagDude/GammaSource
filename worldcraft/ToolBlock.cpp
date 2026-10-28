//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "History.h"
#include "MainFrm.h"
#include "MapDefs.h"
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "Options.h"
#include "StatusBarIDs.h"
#include "ToolBlock.h"
#include "ToolManager.h"
#include "vgui/Cursor.h"
#include "Selection.h"
#include "MapSolid.h"
#include "MapFace.h"
#include "MapWorld.h"
#include "Render2D.h"
#include "GlobalFunctions.h"


class CToolBlockMessageWnd : public CWnd
{
	public:

		bool Create(void);
		void PreMenu2D(CToolBlock *pTool, CMapView2D *pView);

	protected:

		//{{AFX_MSG_MAP(CToolBlockMessageWnd)
		afx_msg void OnCreateObject();
		//}}AFX_MSG
	
		DECLARE_MESSAGE_MAP()

	private:

		CToolBlock *m_pToolBlock;
		CMapView2D *m_pView2D;
};


static CToolBlockMessageWnd s_wndToolMessage;
static const char *g_pszClassName = "ValveEditor_BlockToolWnd";


BEGIN_MESSAGE_MAP(CToolBlockMessageWnd, CWnd)
	//{{AFX_MSG_MAP(CToolMessageWnd)
	ON_COMMAND(ID_CREATEOBJECT, OnCreateObject)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Creates the hidden window that receives context menu commands for the
//			block tool.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolBlockMessageWnd::Create(void)
{
	WNDCLASS wndcls;
	memset(&wndcls, 0, sizeof(WNDCLASS));
    wndcls.lpfnWndProc   = AfxWndProc;
    wndcls.hInstance     = AfxGetInstanceHandle();
    wndcls.lpszClassName = g_pszClassName;

	if (!AfxRegisterClass(&wndcls))
	{
		return(false);
	}

	return(CWnd::CreateEx(0, g_pszClassName, g_pszClassName, 0, CRect(0, 0, 10, 10), NULL, 0) == TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Attaches the block tool to this window before activating the context
//			menu.
//-----------------------------------------------------------------------------
void CToolBlockMessageWnd::PreMenu2D(CToolBlock *pToolBlock, CMapView2D *pView)
{
	Assert(pToolBlock != NULL);
	m_pToolBlock = pToolBlock;
	m_pView2D = pView;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolBlockMessageWnd::OnCreateObject()
{
	m_pToolBlock->CreateMapObject(m_pView2D);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CToolBlock::CToolBlock(void)
{
	// We always show our dimensions when we render.
	SetDrawFlags(GetDrawFlags() | Box3D::boundstext);
	SetDrawColors(Options.colors.clrToolHandle, Options.colors.clrToolBlock);

	m_bPolygonMode = false;
	m_bPolyHover = false;
	m_nPolyAxes[0] = 0; m_nPolyAxes[1] = 1; m_nPolyAxes[2] = 2;
	m_vPolyHover.Init();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CToolBlock::~CToolBlock(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Handles key down events in the 2D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if ( m_bPolygonMode )
	{
		switch ( nChar )
		{
			case VK_RETURN:
				CreatePolygonSolid( pView );
				return true;

			case VK_BACK:
				if ( m_PolyPoints.Count() )
					m_PolyPoints.RemoveMultipleFromTail( 1 );
				m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
				return true;

			case VK_ESCAPE:
				if ( m_PolyPoints.Count() )
				{
					m_PolyPoints.RemoveAll();
					m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
				}
				else
				{
					SetPolygonMode( false );
					ToolManager()->SetTool( TOOL_POINTER );
				}
				return true;
		}
		return false;
	}

	switch (nChar)
	{
		case VK_RETURN:
		{
			if ( !IsEmpty() )
			{
				CreateMapObject(pView);
			}

			return true;
		}

		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles context menu events in the 2D view.
// Input  : Per CWnd::OnContextMenu.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	static CMenu menu, menuCreate;
	static bool bInit = false;

	if (!bInit)
	{
		bInit = true;

		// Create the menu.
		menu.LoadMenu(IDR_POPUPS);
		menuCreate.Attach(::GetSubMenu(menu.m_hMenu, 1));

		// Create the window that handles menu messages.
		s_wndToolMessage.Create();
	}

	if ( !pView->PointInClientRect(vPoint) )
	{
		return false;
	}

	if ( !IsEmpty() )
	{
		if ( HitTest(pView, vPoint, false) )
		{
			CPoint ptScreen( vPoint.x,vPoint.y);
			pView->ClientToScreen(&ptScreen);

			s_wndToolMessage.PreMenu2D(this, pView);
			menuCreate.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y, &s_wndToolMessage);
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	Tool3D::OnLMouseDown3D(pView, nFlags, vPoint);

	//
	// If we have a box, see if they clicked on any part of it.
	//
	if ( !IsEmpty() )
	{
		if ( HitTest( pView, vPoint, true ) )
		{
			// just update current block
			StartTranslation( pView, vPoint, m_LastHitTestHandle );
			return true;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseDown2D(pView, nFlags, vPoint);

	if ( m_bPolygonMode )
	{
		// points from another view start a new outline
		if ( m_PolyPoints.Count() && ( m_nPolyAxes[0] != pView->axHorz || m_nPolyAxes[1] != pView->axVert ) )
		{
			m_PolyPoints.RemoveAll();
		}
		m_nPolyAxes[0] = pView->axHorz;
		m_nPolyAxes[1] = pView->axVert;
		m_nPolyAxes[2] = pView->axThird;

		Vector vecWorld;
		pView->ClientToWorld( vecWorld, vPoint );
		if ( GetConstraints( nFlags ) & constrainSnap )
		{
			m_pDocument->Snap( vecWorld, constrainSnap );
		}
		vecWorld[pView->axThird] = 0;

		// clicking the first point again closes the outline
		if ( m_PolyPoints.Count() >= 3 && VectorsAreEqual( m_PolyPoints[0], vecWorld, 0.01f ) )
		{
			CreatePolygonSolid( pView );
			return true;
		}

		if ( !m_PolyPoints.Count() || !VectorsAreEqual( m_PolyPoints.Tail(), vecWorld, 0.01f ) )
		{
			m_PolyPoints.AddToTail( vecWorld );
		}
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
		return true;
	}

	// If we have a box, see if they clicked on any part of it.
	if ( !IsEmpty() )
	{
		if ( HitTest( pView, vPoint, true ) )
		{
			// just update current block
            StartTranslation( pView, vPoint, m_LastHitTestHandle );
			return true;
		}
	}
		
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseUp2D(pView, nFlags, vPoint);

	if (IsTranslating())
	{
		FinishTranslation(true);
	}

	m_pDocument->UpdateStatusbar();
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseUp3D(pView, nFlags, vPoint);

	if (IsTranslating())
	{
		FinishTranslation(true);
	}

	m_pDocument->UpdateStatusbar();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	if ( m_bPolygonMode )
	{
		Tool3D::OnMouseMove2D( pView, nFlags, vPoint );
		pView->ClientToWorld( m_vPolyHover, vPoint );
		if ( GetConstraints( nFlags ) & constrainSnap )
		{
			m_pDocument->Snap( m_vPolyHover, constrainSnap );
		}
		m_vPolyHover[pView->axThird] = 0;
		m_bPolyHover = m_PolyPoints.Count() && m_nPolyAxes[0] == pView->axHorz && m_nPolyAxes[1] == pView->axVert;
		pView->SetCursor( vgui::dc_crosshair );
		if ( m_bPolyHover )
			m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
		return true;
	}

	vgui::HCursor hCursor = vgui::dc_arrow;

	// Snap it to the grid.
	unsigned int uConstraints = GetConstraints( nFlags );

	Tool3D::OnMouseMove2D(pView, nFlags, vPoint);
							    
	//
	//
	// Convert to world coords.
	//
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);
	
	//
	// Update status bar position display.
	//
	char szBuf[128];

	if ( uConstraints & constrainSnap )
		m_pDocument->Snap(vecWorld,uConstraints);

	sprintf(szBuf, " @%.0f, %.0f ", vecWorld[pView->axHorz], vecWorld[pView->axVert]);
	SetStatusText(SBI_COORDS, szBuf);
	
	if ( IsTranslating() )
	{
		Tool3D::UpdateTranslation( pView, vPoint, uConstraints);

		hCursor =  vgui::dc_none;
	}
	else if ( m_bMouseDragged[MOUSE_LEFT] )
	{
		// Starting a new box. Build a starting point for the drag.
		pView->SetCursor( "Resource/block.cur" );

		Vector vecStart;
		pView->ClientToWorld(vecStart, m_vMouseStart[MOUSE_LEFT] );
		
		// Snap it to the grid.
		if ( uConstraints & constrainSnap )
			m_pDocument->Snap( vecStart, uConstraints);

		// Start the new box with the extents of the last selected thing.

		m_pDocument->GetSelection()->GetLastValidBounds(bmins, bmaxs);

		vecStart[pView->axThird] = bmins[pView->axThird];

		StartNew(pView, vPoint, vecStart, pView->GetViewAxis() * (bmaxs-bmins) );
	}
	else if ( !IsEmpty() )
	{
		// If the cursor is on a handle, set it to a cross.
		if ( HitTest(pView, vPoint, true) )
		{
			hCursor = UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );
		}
	}
	

	if ( hCursor !=  vgui::dc_none )
		pView->SetCursor( hCursor );

	return true;
}

bool CToolBlock::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnMouseMove3D(pView, nFlags, vPoint);

	if ( IsTranslating() )
	{
		unsigned int uConstraints = GetConstraints( nFlags );

		// If they are dragging with a valid handle, update the views.
		Tool3D::UpdateTranslation( pView, vPoint, uConstraints );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles key down events in the 3D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolBlock::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch (nChar)
	{
		case VK_RETURN:
		{
			if ( !IsEmpty() )
			{
				CreateMapObject(pView);
			}

			return true;
		}

		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void CToolBlock::SetPolygonMode( bool bPolygon )
{
	m_bPolygonMode = bPolygon;
	m_PolyPoints.RemoveAll();
	m_bPolyHover = false;
	if ( bPolygon )
	{
		SetEmpty();
		SetStatusText( SBI_PROMPT, "Polygon tool: click the outline points, Enter builds the solid, Backspace removes a point." );
	}
}


void CToolBlock::OnDeactivate()
{
	SetPolygonMode( false );
	Box3D::OnDeactivate();
}


void CToolBlock::RenderTool2D( CRender2D *pRender )
{
	if ( !m_bPolygonMode )
	{
		Box3D::RenderTool2D( pRender );
		return;
	}

	int nPoints = m_PolyPoints.Count();
	if ( !nPoints )
		return;

	pRender->SetDrawColor( 255, 200, 0 );
	for ( int i = 1; i < nPoints; i++ )
	{
		pRender->DrawLine( m_PolyPoints[i - 1], m_PolyPoints[i] );
	}
	if ( m_bPolyHover )
	{
		pRender->SetDrawColor( 160, 120, 0 );
		pRender->DrawLine( m_PolyPoints[nPoints - 1], m_vPolyHover );
		pRender->DrawLine( m_vPolyHover, m_PolyPoints[0] );
	}

	pRender->SetHandleColor( 255, 200, 0 );
	pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_SQUARE );
	for ( int i = 0; i < nPoints; i++ )
	{
		pRender->DrawHandle( m_PolyPoints[i] );
	}
}


static float PolyCross( const Vector2D &o, const Vector2D &a, const Vector2D &b )
{
	return ( a.x - o.x ) * ( b.y - o.y ) - ( a.y - o.y ) * ( b.x - o.x );
}


//-----------------------------------------------------------------------------
// Purpose: Builds a solid from the clicked outline: the outline's convex hull in the
//			view's plane, extruded along the view's depth axis over the current block
//			depth (the selection's depth if nothing is blocked out, else one grid step
//			from 0, at least 1 unit). Undo step: "Create Polygon Solid".
//-----------------------------------------------------------------------------
void CToolBlock::CreatePolygonSolid( CMapView2D *pView )
{
	const int h = m_nPolyAxes[0], v = m_nPolyAxes[1], t = m_nPolyAxes[2];

	// convex hull (Andrew's monotone chain), counter-clockwise in the view's axes
	CUtlVector<Vector2D> pts;
	for ( int i = 0; i < m_PolyPoints.Count(); i++ )
	{
		pts.AddToTail( Vector2D( m_PolyPoints[i][h], m_PolyPoints[i][v] ) );
	}
	for ( int i = 1; i < pts.Count(); i++ )
	{
		for ( int j = i; j > 0 && ( pts[j].x < pts[j - 1].x || ( pts[j].x == pts[j - 1].x && pts[j].y < pts[j - 1].y ) ); j-- )
		{
			V_swap( pts[j], pts[j - 1] );
		}
	}
	CUtlVector<Vector2D> hull;
	for ( int pass = 0; pass < 2; pass++ )
	{
		int nStart = hull.Count();
		for ( int k = 0; k < pts.Count(); k++ )
		{
			const Vector2D &p = pts[pass == 0 ? k : pts.Count() - 1 - k];
			while ( hull.Count() >= nStart + 2 && PolyCross( hull[hull.Count() - 2], hull[hull.Count() - 1], p ) <= 0.01f )
			{
				hull.RemoveMultipleFromTail( 1 );
			}
			hull.AddToTail( p );
		}
		hull.RemoveMultipleFromTail( 1 );
	}

	if ( hull.Count() < 3 || hull.Count() > MAPSOLID_MAX_FACES - 2 )
	{
		AfxMessageBox( hull.Count() < 3 ? "The polygon needs at least three points that are not on one line." : "The polygon has too many sides." );
		return;
	}

	// depth along the view axis
	float flMin = 0, flMax = m_pDocument->GetGridSpacing();
	Vector vSelMins, vSelMaxs;
	if ( !IsEmpty() && bmaxs[t] > bmins[t] )
	{
		flMin = bmins[t]; flMax = bmaxs[t];
	}
	else if ( m_pDocument->GetSelection()->GetBounds( vSelMins, vSelMaxs ) && vSelMaxs[t] > vSelMins[t] )
	{
		flMin = vSelMins[t]; flMax = vSelMaxs[t];
	}
	if ( flMax - flMin < 1.0f )
		flMax = flMin + 1.0f;

	int n = hull.Count();
	CUtlVector<Vector> low, high;
	for ( int i = 0; i < n; i++ )
	{
		Vector p( 0, 0, 0 );
		p[h] = hull[i].x; p[v] = hull[i].y;
		p[t] = flMin; low.AddToTail( p );
		p[t] = flMax; high.AddToTail( p );
	}

	// build it, and if any face points inward build it with the other winding
	for ( int nTry = 0; nTry < 2; nTry++ )
	{
		CMapSolid *pSolid = new CMapSolid;
		CMapFace Face;

		CUtlVector<Vector> cap;
		for ( int i = 0; i < n; i++ ) cap.AddToTail( low[i] );
		cap.AddToTail( low[0] );
		Face.CreateFace( cap.Base(), -n );
		pSolid->AddFace( &Face );

		cap.RemoveAll();
		for ( int i = 0; i < n; i++ ) cap.AddToTail( high[i] );
		cap.AddToTail( high[0] );
		Face.CreateFace( cap.Base(), n );
		pSolid->AddFace( &Face );

		for ( int i = 0; i < n; i++ )
		{
			int j = ( i + 1 ) % n;
			Vector quad[4] = { low[i], low[j], high[j], high[i] };
			Face.CreateFace( quad, 4 );
			pSolid->AddFace( &Face );
		}

		pSolid->CalcBounds();

		Vector vCenter;
		pSolid->GetBoundsCenter( vCenter );
		bool bOutward = true;
		for ( int i = 0; i < pSolid->GetFaceCount() && bOutward; i++ )
		{
			CMapFace *pFace = pSolid->GetFace( i );
			Vector vFaceCenter;
			pFace->GetCenter( vFaceCenter );
			if ( DotProduct( pFace->plane.normal, vFaceCenter - vCenter ) <= 0 )
				bOutward = false;
		}

		if ( !bOutward )
		{
			delete pSolid;
			for ( int i = 0; i < n / 2; i++ )
			{
				V_swap( low[i], low[n - 1 - i] );
				V_swap( high[i], high[n - 1 - i] );
			}
			continue;
		}

		pSolid->InitializeTextureAxes( Options.GetTextureAlignment(), INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE );
		pSolid->SetTexture( GetDefaultTextureName() );

		GetHistory()->MarkUndoPosition( m_pDocument->GetSelection()->GetList(), "Create Polygon Solid" );
		m_pDocument->AddObjectToWorld( pSolid );
		GetHistory()->KeepNew( pSolid );
		m_pDocument->SelectObject( pSolid, scClear | scSelect | scSaveChanges );
		m_pDocument->SetModifiedFlag();

		m_PolyPoints.RemoveAll();
		m_bPolyHover = false;
		m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL | MAPVIEW_UPDATE_OBJECTS );
		return;
	}

	AfxMessageBox( "Could not build a solid from this polygon." );
}


void CToolBlock::OnEscape(void)
{
	//
	// If we have nothing blocked, go back to the pointer tool.
	//
	if ( IsEmpty() )
	{
		ToolManager()->SetTool(TOOL_POINTER);
	}
	//
	// We're blocking out a region, so clear it.
	//
	else
	{
		SetEmpty();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates a solid in the given view.
//-----------------------------------------------------------------------------
void CToolBlock::CreateMapObject(CMapView *pView)
{
	CMapWorld *pWorld = m_pDocument->GetMapWorld();

	if (!(bmaxs[0] - bmins[0]) || !(bmaxs[1] - bmins[1]) || !(bmaxs[2] - bmins[2]))
	{
		AfxMessageBox("The box is empty.");
		SetEmpty();
		return;
	}

	BoundBox NewBox = *this;
	if (Options.view2d.bOrientPrimitives)
	{
		switch (pView->GetDrawType())
		{
			case VIEW2D_XY:
			{
				break;
			}

			case VIEW2D_YZ:
			{
				NewBox.Rotate90(AXIS_Y);
				break;
			}

			case VIEW2D_XZ:
			{
				NewBox.Rotate90(AXIS_X);
				break;
			}
		}
	}

	// Create the object within the given bounding box.
	CMapClass *pObject = GetMainWnd()->m_ObjectBar.CreateInBox(&NewBox, pView);
	if (pObject == NULL)
	{
		SetEmpty();
		return;
	}

	m_pDocument->ExpandObjectKeywords(pObject, pWorld);

	GetHistory()->MarkUndoPosition(m_pDocument->GetSelection()->GetList(), "New Object");

	m_pDocument->AddObjectToWorld(pObject);

	//
	// Do we need to rotate this object based on the view we created it in?
	//
	if (Options.view2d.bOrientPrimitives)
	{
		Vector center;
		pObject->GetBoundsCenter( center );

		QAngle angles(0, 0, 0);
		switch (pView->GetDrawType())
		{
			case VIEW2D_XY:
			{
				break;
			}
			
			case VIEW2D_YZ:
			{
 				angles[1] = 90.f;
				pObject->TransRotate(center, angles);
				break;
			}

			case VIEW2D_XZ:
			{
				angles[0] = 90.f;
				pObject->TransRotate(center, angles);
				break;
			}
		}
	}

	GetHistory()->KeepNew(pObject);

	// Select the new object.
	m_pDocument->SelectObject(pObject, scClear|scSelect|scSaveChanges);

	m_pDocument->SetModifiedFlag();

	SetEmpty();
	ResetBounds();
}





