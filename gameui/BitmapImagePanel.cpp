//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BitmapImagePanel.h"
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui; // HL2MODERN_MODEL_UI_SAFETY: only one memdbgon include

CBitmapImagePanel::CBitmapImagePanel( Panel *parent, char const *panelName, 
	char const *filename /*= NULL*/ ) : Panel( parent, panelName )
{
	m_szTexture[ 0 ] = 0;
	m_bUploaded = false;
	m_nTextureId = -1;

	SetBounds( 0, 0, 100, 100 );

	if ( filename && filename[ 0 ] )
	{
		strncpy(m_szTexture, filename, sizeof(m_szTexture)-1);
	m_szTexture[sizeof(m_szTexture)-1]=0;
	}
}

void CBitmapImagePanel::PaintBackground()
{
	if (!m_szTexture[0])
		return;

	if ( !m_bUploaded )
		forceUpload();

	int w, h;
	GetSize( w, h );
	surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
	surface()->DrawSetTexture( m_nTextureId );
	// HL2MODERN_PREVIEW_ASPECT: fit the complete image without stretching it.
	int imageW=0, imageH=0;
	surface()->DrawGetTextureSize(m_nTextureId,imageW,imageH);
	if (imageW>0 && imageH>0 && w>0 && h>0)
	{
		const float scaleX=(float)w/(float)imageW;
		const float scaleY=(float)h/(float)imageH;
		const float scale=scaleX<scaleY?scaleX:scaleY;
		const int outW=(int)(imageW*scale);
		const int outH=(int)(imageH*scale);
		surface()->DrawTexturedRect((w-outW)/2,(h-outH)/2,(w+outW)/2,(h+outH)/2);
	}
}

void CBitmapImagePanel::setTexture( char const *filename )
{
	if (!filename) filename="";
	strncpy(m_szTexture, filename, sizeof(m_szTexture)-1);
	m_szTexture[sizeof(m_szTexture)-1]=0;
	if (!m_szTexture[0])
	{
		// Avoid uploading a zero-length filename when the selected image disappears.
		m_bUploaded = false;
		return;
	}

	if ( m_bUploaded )
	{
		forceReload();
	}
	else
	{
		forceUpload();
	}
}

void CBitmapImagePanel::forceUpload()
{
	if ( !m_szTexture[ 0 ] )
		return;

	m_bUploaded = true;

	m_nTextureId = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nTextureId, m_szTexture, false, true);
}

void CBitmapImagePanel::forceReload( void )
{
	if ( !m_bUploaded )
		return;

	// Force texture to re-upload to video card
	surface()->DrawSetTextureFile( m_nTextureId, m_szTexture, false, true);
}
