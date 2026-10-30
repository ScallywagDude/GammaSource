//========= GammaSource ============//
//
// GENERATED from materialsystem/shaderapidx11 by materialsystem/shaderapidx12/make_dx12.py - edit the
// DX11 files and regenerate, so the backends stay one implementation.
//
//=============================================================================//
//========= GammaSource ============//
//
// Native Direct3D 12 drawing for shaderapidx12 (kept free of Source headers):
// textures, render state, matrices and a textured, vertex-coloured draw path that the
// materials' passes go through.
//
//=============================================================================//
#ifndef DX12RENDER_H
#define DX12RENDER_H
#pragma once

struct Dx12Vertex_t
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
struct Dx12Light_t
{
	int m_nType;			// 0 off, 1 point, 2 directional, 3 spot (LightType_t)
	float m_Color[3];		// linear
	float m_Pos[3];
	float m_Dir[3];
	float m_flRange;
	float m_flAtten[3];		// constant, linear, quadratic
	float m_flThetaDot, m_flPhiDot, m_flFalloff;
};

struct Dx12State_t
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

enum Dx12Primitive_t
{
	DX12_POINTS, DX12_LINES, DX12_TRIANGLES, DX12_TRIANGLE_STRIP, DX12_LINE_STRIP
};

bool Dx12R_Init();
void Dx12R_Shutdown();

int  Dx12R_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget = false, bool bCube = false );
void Dx12R_TexImageFace( int hTexture, int nFace, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA );
bool Dx12R_IsCubeTexture( int hTexture );
// envmap reflection for this pass (the cube bound on stage 1 or 2): tint rgb, nMask 1 = base alpha masks it
void Dx12R_SetEnvmap( bool bEnable, const float *pTint, int nMask, const float *pEyePos );
void Dx12R_DeleteTexture( int hTexture );
bool Dx12R_IsTexture( int hTexture );
void Dx12R_TexImage( int hTexture, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA );
void Dx12R_TexSubImage( int hTexture, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA );
void Dx12R_TexWrap( int hTexture, int nCoord, bool bClamp );
// CPU copy of mip 0 for textures that get locked (lightmap pages): a lock must
// see what is already there, since the engine rewrites only part of a page
unsigned char *Dx12R_LockShadow( int hTexture, int &nPitch, int &nWidth, int &nHeight );
void Dx12R_UploadRect( int hTexture, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch );
void Dx12R_BindTexture( int nStage, int hTexture );
int Dx12R_GetBoundTexture( int nStage );
void Dx12R_BindLightmap( int hTexture );	// 0: no lightmap for this pass
// lit passes (vertex lit models): colour = vertex colour * (ambient cube + lights)
// the Shadow shader's projection pass: colour = lerp( 1, shadowColor, texture alpha - vertex alpha )
// skinning: up to 53 bone-to-world 3x4 matrices (rows); bone 0 also sets the model matrix
enum { DX12_MAX_BONES = 53 };
void Dx12R_SetBoneMatrices( const float ( *pBones )[12], int nCount );
// how a pass uses vertex colour: Source's generic shaders ignore it unless the material
// has $vertexcolor / $vertexalpha; pColor is the material's $color and $alpha
void Dx12R_SetVertexColorUse( bool bColor, bool bAlpha, const float *pColor );
// the Water shader: refraction (stage 0) and reflection (stage 2) render targets looked up
// in screen space, tinted and mixed; pTints = refract tint, reflect tint (linear rgb + amount)
void Dx12R_SetWaterPass( bool bWater, int hReflection, const float *pRefractTint, const float *pReflectTint );
// world-space clip planes ( nx, ny, nz, d ): points with dot( n, p ) < d are clipped
void Dx12R_SetClipPlane( int nIndex, const float *pPlane, bool bEnable );
// occlusion queries: handles are 1-based; the result is the visible pixel count, -1 while pending
int  Dx12R_CreateQuery();
void Dx12R_DestroyQuery( int hQuery );
void Dx12R_BeginQuery( int hQuery );
void Dx12R_EndQuery( int hQuery );
int  Dx12R_GetQueryResult( int hQuery, bool bFlush );
void Dx12R_SetShadowPass( bool bShadow, const float *pShadowColor );
void Dx12R_SetLighting( bool bLit, const float ambientCube[6][3], int nLights, const Dx12Light_t *pLights );

void Dx12R_SetState( const Dx12State_t &state );
void Dx12R_SetMatrices( const float *pModel, const float *pView, const float *pProj );	// row-vector 4x4
void Dx12R_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ );
void Dx12R_ClearDepth();
// render targets: hColor <= 0 is the back buffer
void Dx12R_SetRenderTarget( int hColor );
void Dx12R_Clear( bool bColor, bool bDepth );
void Dx12R_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy );
void Dx12R_GetTextureSize( int hTexture, int &nWidth, int &nHeight );
bool Dx12R_IsRenderTargetTexture( int hTexture );
// pCacheKey/nVersion: a static mesh's vertices are kept on the GPU and reused while the
// version matches (pVerts may then be NULL); pass NULL for data used once
void Dx12R_Draw( Dx12Primitive_t type, const Dx12Vertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices,
	const void *pCacheKey = 0, int nVersion = 0 );
bool Dx12R_HasCachedVertices( const void *pCacheKey, int nVersion );
void Dx12R_ReleaseCachedVertices( const void *pCacheKey );

#endif // DX12RENDER_H
