//========= GammaSource ============//
//
// GENERATED from materialsystem/shaderapidx11 by materialsystem/shaderapigl/make_gl.py - edit the
// DX11 files and regenerate, so the backends stay one implementation.
//
//=============================================================================//
//========= GammaSource ============//
//
// Native OpenGL drawing for shaderapigl (kept free of Source headers):
// textures, render state, matrices and a textured, vertex-coloured draw path that the
// materials' passes go through.
//
//=============================================================================//
#ifndef GLRRENDER_H
#define GLRRENDER_H
#pragma once

struct GlRVertex_t
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
struct GlRLight_t
{
	int m_nType;			// 0 off, 1 point, 2 directional, 3 spot (LightType_t)
	float m_Color[3];		// linear
	float m_Pos[3];
	float m_Dir[3];
	float m_flRange;
	float m_flAtten[3];		// constant, linear, quadratic
	float m_flThetaDot, m_flPhiDot, m_flFalloff;
};

struct GlRState_t
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

enum GlRPrimitive_t
{
	GLR_POINTS, GLR_LINES, GLR_TRIANGLES, GLR_TRIANGLE_STRIP, GLR_LINE_STRIP
};

bool GlR_Init();
void GlR_Shutdown();

int  GlR_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget = false, bool bCube = false );
void GlR_TexImageFace( int hTexture, int nFace, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA );
bool GlR_IsCubeTexture( int hTexture );
// envmap reflection for this pass (the cube bound on stage 1 or 2): tint rgb, nMask 1 = base alpha masks it
void GlR_SetEnvmap( bool bEnable, const float *pTint, int nMask, const float *pEyePos );
void GlR_DeleteTexture( int hTexture );
bool GlR_IsTexture( int hTexture );
void GlR_TexImage( int hTexture, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA );
void GlR_TexSubImage( int hTexture, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA );
void GlR_TexWrap( int hTexture, int nCoord, bool bClamp );
// CPU copy of mip 0 for textures that get locked (lightmap pages): a lock must
// see what is already there, since the engine rewrites only part of a page
unsigned char *GlR_LockShadow( int hTexture, int &nPitch, int &nWidth, int &nHeight );
void GlR_UploadRect( int hTexture, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch );
void GlR_BindTexture( int nStage, int hTexture );
int GlR_GetBoundTexture( int nStage );
void GlR_BindLightmap( int hTexture );	// 0: no lightmap for this pass
// lit passes (vertex lit models): colour = vertex colour * (ambient cube + lights)
// the Shadow shader's projection pass: colour = lerp( 1, shadowColor, texture alpha - vertex alpha )
// skinning: up to 53 bone-to-world 3x4 matrices (rows); bone 0 also sets the model matrix
enum { GLR_MAX_BONES = 53 };
void GlR_SetBoneMatrices( const float ( *pBones )[12], int nCount );
// how a pass uses vertex colour: Source's generic shaders ignore it unless the material
// has $vertexcolor / $vertexalpha; pColor is the material's $color and $alpha
void GlR_SetVertexColorUse( bool bColor, bool bAlpha, const float *pColor );
// the Water shader: refraction (stage 0) and reflection (stage 2) render targets looked up
// in screen space, tinted and mixed; pTints = refract tint, reflect tint (linear rgb + amount)
void GlR_SetWaterPass( bool bWater, int hReflection, const float *pRefractTint, const float *pReflectTint );
// world-space clip planes ( nx, ny, nz, d ): points with dot( n, p ) < d are clipped
void GlR_SetClipPlane( int nIndex, const float *pPlane, bool bEnable );
// occlusion queries: handles are 1-based; the result is the visible pixel count, -1 while pending
int  GlR_CreateQuery();
void GlR_DestroyQuery( int hQuery );
void GlR_BeginQuery( int hQuery );
void GlR_EndQuery( int hQuery );
int  GlR_GetQueryResult( int hQuery, bool bFlush );
void GlR_SetShadowPass( bool bShadow, const float *pShadowColor );
void GlR_SetLighting( bool bLit, const float ambientCube[6][3], int nLights, const GlRLight_t *pLights );

void GlR_SetState( const GlRState_t &state );
void GlR_SetMatrices( const float *pModel, const float *pView, const float *pProj );	// row-vector 4x4
void GlR_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ );
void GlR_ClearDepth();
// render targets: hColor <= 0 is the back buffer
void GlR_SetRenderTarget( int hColor );
void GlR_Clear( bool bColor, bool bDepth );
void GlR_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy );
void GlR_GetTextureSize( int hTexture, int &nWidth, int &nHeight );
bool GlR_IsRenderTargetTexture( int hTexture );
// pCacheKey/nVersion: a static mesh's vertices are kept on the GPU and reused while the
// version matches (pVerts may then be NULL); pass NULL for data used once
void GlR_Draw( GlRPrimitive_t type, const GlRVertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices,
	const void *pCacheKey = 0, int nVersion = 0 );
bool GlR_HasCachedVertices( const void *pCacheKey, int nVersion );
void GlR_ReleaseCachedVertices( const void *pCacheKey );

#endif // GLRRENDER_H
