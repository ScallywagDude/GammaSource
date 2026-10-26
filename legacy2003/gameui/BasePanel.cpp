//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>

#include "BasePanel.h"
#include "Taskbar.h"
#include "EngineInterface.h"
#if defined(_WIN32) && !defined(_WIN64)
#include "FileSystem.h"
#include <KeyValues.h>
#endif

#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>

#include "GameConsole.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBasePanel::CBasePanel() : Panel(NULL, "BasePanel")
{
	m_eBackgroundState = BACKGROUND_NONE;
#if defined(_WIN32) && !defined(_WIN64)
	m_LoadingImage.imageID = -1;
	m_LoadingImage.width = m_LoadingImage.height = 0;
	m_LoadingArtWide = m_LoadingArtTall = 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Notifies the task bar that a new top-level panel has been created
//-----------------------------------------------------------------------------
void CBasePanel::OnChildAdded(VPANEL child)
{
	if (g_pTaskbar)
	{
		g_pTaskbar->AddTask(child);
	}
}

//-----------------------------------------------------------------------------
// Purpose: paints the main background image
//-----------------------------------------------------------------------------
void CBasePanel::PaintBackground()
{
#if defined(_WIN32) && !defined(_WIN64)
	// Loading can begin while the engine still reports the previous level.
	if (m_eBackgroundState == BACKGROUND_BLACK || m_eBackgroundState == BACKGROUND_LOADING)
	{
		DrawLoadingImage();
		return;
	}
#endif
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0])
	{
		// render filled background in game
		int swide, stall;
		surface()->GetScreenSize(swide, stall);
		surface()->DrawSetColor(0, 0, 0, 128);
		surface()->DrawFilledRect(0, 0, swide, stall);		
		return;	
	}

	switch (m_eBackgroundState)
	{
	case BACKGROUND_BLACK:
		{
			// if the loading dialog is visible, draw the background black
			int swide, stall;
			surface()->GetScreenSize(swide, stall);
			surface()->DrawSetColor(0, 0, 0, 255);
			surface()->DrawFilledRect(0, 0, swide, stall);		
		}
		break;

	case BACKGROUND_LOADING:
		DrawBackgroundImage();
		break;

	case BACKGROUND_DESKTOPIMAGE:
		DrawBackgroundImage();
		break;
		
	case BACKGROUND_LOADINGTRANSITION:
		{
		}
		break;

	case BACKGROUND_NONE:
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads background texture
//-----------------------------------------------------------------------------
void CBasePanel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	// turn on hardware filtering if we're scaling the images
	int wide, tall;
	surface()->GetScreenSize(wide, tall);
	bool hardwareFilter = false; //(wide != 800);

	bimage_t &bimage = m_ImageID[0][0];
	bimage.imageID = surface()->CreateNewTextureID();

	char filename[512];
	sprintf(filename, "console/console_background" );
	surface()->DrawSetTextureFile(bimage.imageID, filename, hardwareFilter, false);
	surface()->DrawGetTextureSize(bimage.imageID, bimage.width, bimage.height);

#if defined(_WIN32) && !defined(_WIN64)
	if (m_LoadingImage.imageID < 0)
		m_LoadingImage.imageID = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile(m_LoadingImage.imageID, "console/gammasource_loading", true, false);
	surface()->DrawGetTextureSize(m_LoadingImage.imageID, m_LoadingImage.width, m_LoadingImage.height);
	m_LoadingArtWide = m_LoadingArtTall = 0;
	KeyValues *loadingInfo = new KeyValues("GammaSourceLoading");
	if (loadingInfo->LoadFromFile(filesystem(), "resource/gammasource_loading.res", "GAME"))
	{
		m_LoadingArtWide = loadingInfo->GetInt("width", 0);
		m_LoadingArtTall = loadingInfo->GetInt("height", 0);
	}
	loadingInfo->deleteThis();
#endif

/*
	for (int y = 0; y < BACKGROUND_ROWS; y++)
	{
		for (int x = 0; x < BACKGROUND_COLUMNS; x++)
		{
			bimage_t &bimage = m_ImageID[y][x];
			bimage.imageID = surface()->CreateNewTextureID();

			char filename[512];
			sprintf(filename, "resource/background/800_%d_%c_loading", y + 1, 'a' + x);
			surface()->DrawSetTextureFile(bimage.imageID, filename, hardwareFilter, false);
			surface()->DrawGetTextureSize(bimage.imageID, bimage.width, bimage.height);
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: sets how the game background should render
//-----------------------------------------------------------------------------
void CBasePanel::SetBackgroundRenderState(EBackgroundState state)
{
	m_eBackgroundState = state;
}

//-----------------------------------------------------------------------------
// Purpose: draws the background desktop image
//-----------------------------------------------------------------------------
void CBasePanel::DrawBackgroundImage()
{
//	int xpos, ypos;
	int wide, tall;
	GetSize(wide, tall);

	// work out scaling factors
	int swide, stall;
	surface()->GetScreenSize(swide, stall);
	float xScale, yScale;
//	xScale = swide / 800.0f;
//	yScale = stall / 600.0f;
	xScale = 1.0f;
	yScale = 1.0f;
/*
	// iterate and draw all the background pieces
	ypos = 0;
	for (int y = 0; y < BACKGROUND_ROWS; y++)
	{
		xpos = 0;
		for (int x = 0; x < BACKGROUND_COLUMNS; x++)
		{
			bimage_t &bimage = m_ImageID[y][x];

			int dx = (int)ceil(xpos * xScale);
			int dy = (int)ceil(ypos * yScale);
			int dw = (int)ceil((xpos + bimage.width) * xScale);
			int dt = (int)ceil((ypos + bimage.height) * yScale);

			if (x == 0)
			{
				dx = 0;
			}
			if (y == 0)
			{
				dy = 0;
			}

  */
			bimage_t &bimage = m_ImageID[0][0];
			// draw the color image only if the mono image isn't yet fully opaque
			surface()->DrawSetColor(255, 255, 255, 255);
			surface()->DrawSetTexture(bimage.imageID);
			surface()->DrawTexturedRect(0, 0, wide, tall);

		/*	xpos += bimage.width;
		}
		ypos += m_ImageID[y][0].height;
	}
	*/
}

#if defined(_WIN32) && !defined(_WIN64)
// The PNG pixels occupy this rectangle in a power-of-two texture. Drawing its
// UV subrectangle avoids stretching either the artwork or its storage padding.
void CBasePanel::DrawLoadingImage()
{
	int wide, tall;
	GetSize(wide, tall);
	surface()->DrawSetColor(0, 0, 0, 255);
	surface()->DrawFilledRect(0, 0, wide, tall);
	const int artWide = m_LoadingArtWide, artTall = m_LoadingArtTall;
	if (wide <= 0 || tall <= 0 || m_LoadingImage.imageID < 0 ||
		artWide <= 0 || artTall <= 0 ||
		m_LoadingImage.width < artWide || m_LoadingImage.height < artTall)
		return;
	int drawWide = wide;
	int drawTall = (int)((double)wide * artTall / artWide + 0.5);
	if (drawTall > tall)
	{
		drawTall = tall;
		drawWide = (int)((double)tall * artWide / artTall + 0.5);
	}
	const int left = (wide - drawWide) / 2;
	const int top = (tall - drawTall) / 2;
	surface()->DrawSetColor(255, 255, 255, 255);
	surface()->DrawSetTexture(m_LoadingImage.imageID);
	surface()->DrawTexturedSubRect(left, top, left + drawWide, top + drawTall,
		0.0f, 0.0f, (float)artWide / m_LoadingImage.width, (float)artTall / m_LoadingImage.height);
}
#endif
