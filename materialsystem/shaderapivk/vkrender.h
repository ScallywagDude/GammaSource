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
	float m_Normal[3];		// model space, for lit passes
	unsigned char m_BoneIndex[4];
	float m_BoneWeight[2];	// the third weight is 1 - the others
};

// a light for lit (model) passes, world space
struct VkRLight_t
{
	int m_nType;			// 0 off, 1 point, 2 directional, 3 spot (LightType_t)
	float m_Color[3];		// linear
	float m_Pos[3];
	float m_Dir[3];
	float m_flRange;
	float m_flAtten[3];		// constant, linear, quadratic
	float m_flThetaDot, m_flPhiDot, m_flFalloff;
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
	bool m_bNormals;		// the pass's vertex format has normals (lit shaders)
	bool m_bDecalOffset;	// SHADER_POLYOFFSET_DECAL
	int m_nBoneWeights;		// skinned: vertices blend this many bone matrices
	bool m_bVertexColor;	// the pass's vertex format has colours
	bool m_bNoColorWrites;	// EnableColorWrites( false ): depth-only / occlusion proxy passes
	bool m_bNoAlphaWrites;
};

enum VkRPrimitive_t
{
	VKR_POINTS, VKR_LINES, VKR_TRIANGLES, VKR_TRIANGLE_STRIP, VKR_LINE_STRIP
};

bool VkR_Init();
void VkR_Shutdown();

int  VkR_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget = false, bool bCube = false );
void VkR_TexImageFace( int hTexture, int nFace, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA );
bool VkR_IsCubeTexture( int hTexture );
// envmap reflection for this pass (the cube bound on stage 1 or 2): tint rgb, nMask 1 = base alpha masks it
void VkR_SetEnvmap( bool bEnable, const float *pTint, int nMask, const float *pEyePos );
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
int VkR_GetBoundTexture( int nStage );
void VkR_BindLightmap( int hTexture );	// 0: no lightmap for this pass
// lit passes (vertex lit models): colour = vertex colour * (ambient cube + lights)
// the Shadow shader's projection pass: colour = lerp( 1, shadowColor, texture alpha - vertex alpha )
// skinning: up to 53 bone-to-world 3x4 matrices (rows); bone 0 also sets the model matrix
enum { VKR_MAX_BONES = 53 };
void VkR_SetBoneMatrices( const float ( *pBones )[12], int nCount );
// how a pass uses vertex colour: Source's generic shaders ignore it unless the material
// has $vertexcolor / $vertexalpha; pColor is the material's $color and $alpha
void VkR_SetVertexColorUse( bool bColor, bool bAlpha, const float *pColor );
// the Water shader: refraction (stage 0) and reflection (stage 2) render targets looked up
// in screen space, tinted and mixed; pTints = refract tint, reflect tint (linear rgb + amount)
void VkR_SetWaterPass( bool bWater, int hReflection, const float *pRefractTint, const float *pReflectTint );
// world-space clip planes ( nx, ny, nz, d ): points with dot( n, p ) < d are clipped
void VkR_SetClipPlane( int nIndex, const float *pPlane, bool bEnable );
// occlusion queries: handles are 1-based; the result is the visible pixel count, -1 while pending
int  VkR_CreateQuery();
void VkR_DestroyQuery( int hQuery );
void VkR_BeginQuery( int hQuery );
void VkR_EndQuery( int hQuery );
int  VkR_GetQueryResult( int hQuery, bool bFlush );
void VkR_SetShadowPass( bool bShadow, const float *pShadowColor );
void VkR_SetLighting( bool bLit, const float ambientCube[6][3], int nLights, const VkRLight_t *pLights );

void VkR_SetState( const VkRState_t &state );
void VkR_SetMatrices( const float *pModel, const float *pView, const float *pProj );	// row-vector 4x4
void VkR_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ );
void VkR_ClearDepth();
// render targets: hColor <= 0 is the back buffer
void VkR_SetRenderTarget( int hColor );
void VkR_Clear( bool bColor, bool bDepth );
void VkR_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy );
void VkR_GetTextureSize( int hTexture, int &nWidth, int &nHeight );
bool VkR_IsRenderTargetTexture( int hTexture );
// pCacheKey/nVersion: a static mesh's vertices are kept on the GPU and reused while the
// version matches (pVerts may then be NULL); pass NULL for data used once
void VkR_Draw( VkRPrimitive_t type, const VkRVertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices,
	const void *pCacheKey = 0, int nVersion = 0 );
bool VkR_HasCachedVertices( const void *pCacheKey, int nVersion );
void VkR_ReleaseCachedVertices( const void *pCacheKey );

#endif // VKRENDER_H
