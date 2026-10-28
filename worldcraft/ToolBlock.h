//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOLBLOCK_H
#define TOOLBLOCK_H
#ifdef _WIN32
#pragma once
#endif


#include "Box3D.h"
#include "ToolInterface.h"


class CToolBlockMessageWnd;


class CToolBlock : public Box3D
{

friend class CToolBlockMessageWnd;

public:

	CToolBlock();
	~CToolBlock();

	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_BLOCK; }

	virtual bool OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint) ;
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual bool OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);

	virtual void OnDeactivate();
	virtual void RenderTool2D(CRender2D *pRender);

	// Hammer++ polygon tool: click the outline in a 2D view, Enter builds a solid from it
	void SetPolygonMode( bool bPolygon );
	bool IsPolygonMode() const { return m_bPolygonMode; }

private:

	void CreatePolygonSolid( CMapView2D *pView );

	bool m_bPolygonMode;
	CUtlVector<Vector> m_PolyPoints;	// outline points, in the view they were clicked in
	int m_nPolyAxes[3];					// that view's horizontal, vertical and depth axes
	Vector m_vPolyHover;				// snapped cursor position, for the rubber band
	bool m_bPolyHover;

	void CreateMapObject(CMapView *pView);
	void OnEscape();

	void SetBlockCursor();
};


#endif // TOOLBLOCK_H
