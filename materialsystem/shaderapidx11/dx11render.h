//========= GammaSource ============//
//
// Native Direct3D 11 drawing for shaderapidx11 (kept free of Source headers):
// textures, render state, matrices and a textured, vertex-coloured draw path that the
// materials' passes go through.
//
//=============================================================================//
#ifndef DX11RENDER_H
#define DX11RENDER_H
#pragma once

struct Dx11Vertex_t
{
	float m_Pos[3];
	unsigned int m_Color;	// RGBA bytes in memory order
	float m_UV[2];
};

struct Dx11State_t
{
	bool m_bBlend;
	int m_nSrcBlend;		// ShaderBlendFactor_t
	int m_nDstBlend;
	bool m_bAlphaTest;
	float m_flAlphaRef;
	bool m_bDepthTest;
	bool m_bDepthWrite;
	int m_nDepthFunc;		// ShaderDepthFunc_t
	bool m_bCull;
};

enum Dx11Primitive_t
{
	DX11_POINTS, DX11_LINES, DX11_TRIANGLES, DX11_TRIANGLE_STRIP, DX11_LINE_STRIP
};

bool Dx11R_Init();
void Dx11R_Shutdown();

int  Dx11R_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget = false );
void Dx11R_DeleteTexture( int hTexture );
bool Dx11R_IsTexture( int hTexture );
void Dx11R_TexImage( int hTexture, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA );
void Dx11R_TexSubImage( int hTexture, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA );
void Dx11R_TexWrap( int hTexture, int nCoord, bool bClamp );
void Dx11R_BindTexture( int nStage, int hTexture );

void Dx11R_SetState( const Dx11State_t &state );
void Dx11R_SetMatrices( const float *pModel, const float *pView, const float *pProj );	// row-vector 4x4
void Dx11R_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ );
void Dx11R_ClearDepth();
// render targets: hColor <= 0 is the back buffer
void Dx11R_SetRenderTarget( int hColor );
void Dx11R_Clear( bool bColor, bool bDepth );
void Dx11R_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy );
void Dx11R_GetTextureSize( int hTexture, int &nWidth, int &nHeight );
void Dx11R_Draw( Dx11Primitive_t type, const Dx11Vertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices );

#endif // DX11RENDER_H
