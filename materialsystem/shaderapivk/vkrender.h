//========= GammaSource ============//
//
// GENERATED from materialsystem/shaderapidx11 by materialsystem/shaderapivk/make_vk.py - edit the DX11
// files and regenerate, so the two backends stay one implementation.
//
//=============================================================================//
//========= GammaSource ============//
//
// Native Vulkan drawing for shaderapivk (kept free of Source headers):
// textures, render state, matrices and a textured, vertex-coloured draw path that the
// materials' passes go through.
//
//=============================================================================//
#ifndef VKRENDER_H
#define VKRENDER_H
#pragma once

struct VkRVertex_t
{
	float m_Pos[3];
	unsigned int m_Color;	// RGBA bytes in memory order
	float m_UV[2];
	float m_UV2[2];			// lightmap coordinates
};

struct VkRState_t
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

enum VkRPrimitive_t
{
	VKR_POINTS, VKR_LINES, VKR_TRIANGLES, VKR_TRIANGLE_STRIP, VKR_LINE_STRIP
};

bool VkR_Init();
void VkR_Shutdown();

int  VkR_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget = false );
void VkR_DeleteTexture( int hTexture );
bool VkR_IsTexture( int hTexture );
void VkR_TexImage( int hTexture, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA );
void VkR_TexSubImage( int hTexture, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA );
void VkR_TexWrap( int hTexture, int nCoord, bool bClamp );
// CPU copy of mip 0 for textures that get locked (lightmap pages): a lock must
// see what is already there, since the engine rewrites only part of a page
unsigned char *VkR_LockShadow( int hTexture, int &nPitch, int &nWidth, int &nHeight );
void VkR_UploadRect( int hTexture, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch );
void VkR_BindTexture( int nStage, int hTexture );
void VkR_BindLightmap( int hTexture );	// 0: no lightmap for this pass

void VkR_SetState( const VkRState_t &state );
void VkR_SetMatrices( const float *pModel, const float *pView, const float *pProj );	// row-vector 4x4
void VkR_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ );
void VkR_ClearDepth();
// render targets: hColor <= 0 is the back buffer
void VkR_SetRenderTarget( int hColor );
void VkR_Clear( bool bColor, bool bDepth );
void VkR_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy );
void VkR_GetTextureSize( int hTexture, int &nWidth, int &nHeight );
// pCacheKey/nVersion: a static mesh's vertices are kept on the GPU and reused while the
// version matches (pVerts may then be NULL); pass NULL for data used once
void VkR_Draw( VkRPrimitive_t type, const VkRVertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices,
	const void *pCacheKey = 0, int nVersion = 0 );
bool VkR_HasCachedVertices( const void *pCacheKey, int nVersion );
void VkR_ReleaseCachedVertices( const void *pCacheKey );

#endif // VKRENDER_H
