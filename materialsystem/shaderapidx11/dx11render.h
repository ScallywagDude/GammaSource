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
	float m_UV2[2];			// lightmap coordinates
	float m_Normal[3];		// model space, for lit passes
	unsigned char m_BoneIndex[4];
	float m_BoneWeight[2];	// the third weight is 1 - the others
};

// a light for lit (model) passes, world space
struct Dx11Light_t
{
	int m_nType;			// 0 off, 1 point, 2 directional, 3 spot (LightType_t)
	float m_Color[3];		// linear
	float m_Pos[3];
	float m_Dir[3];
	float m_flRange;
	float m_flAtten[3];		// constant, linear, quadratic
	float m_flThetaDot, m_flPhiDot, m_flFalloff;
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
	bool m_bNormals;		// the pass's vertex format has normals (lit shaders)
	bool m_bDecalOffset;	// SHADER_POLYOFFSET_DECAL
	int m_nBoneWeights;		// skinned: vertices blend this many bone matrices
	bool m_bVertexColor;	// the pass's vertex format has colours
	bool m_bNoColorWrites;	// EnableColorWrites( false ): depth-only / occlusion proxy passes
	bool m_bNoAlphaWrites;
	int m_nFogMode;			// ShaderFogMode_t
};

enum Dx11Primitive_t
{
	DX11_POINTS, DX11_LINES, DX11_TRIANGLES, DX11_TRIANGLE_STRIP, DX11_LINE_STRIP
};

bool Dx11R_Init();
void Dx11R_Shutdown();

int  Dx11R_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget = false, bool bCube = false );
void Dx11R_TexImageFace( int hTexture, int nFace, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA );
bool Dx11R_IsCubeTexture( int hTexture );
// envmap reflection for this pass (the cube bound on stage 1 or 2): tint rgb; nMask 0 none, 1 base alpha,
// 2 hMask's rgb ($envmapmask), 3 hMask's alpha (the normal map's: $normalmapalphaenvmapmask)
void Dx11R_SetEnvmap( bool bEnable, const float *pTint, int nMask, const float *pEyePos, int hMask = 0 );
void Dx11R_DeleteTexture( int hTexture );
bool Dx11R_IsTexture( int hTexture );
void Dx11R_TexImage( int hTexture, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA );
void Dx11R_TexSubImage( int hTexture, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA );
void Dx11R_TexWrap( int hTexture, int nCoord, bool bClamp );
// CPU copy of mip 0 for textures that get locked (lightmap pages): a lock must
// see what is already there, since the engine rewrites only part of a page
unsigned char *Dx11R_LockShadow( int hTexture, int &nPitch, int &nWidth, int &nHeight );
void Dx11R_UploadRect( int hTexture, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch );
void Dx11R_BindTexture( int nStage, int hTexture );
int Dx11R_GetBoundTexture( int nStage );
void Dx11R_BindLightmap( int hTexture );	// 0: no lightmap for this pass
// lit passes (vertex lit models): colour = vertex colour * (ambient cube + lights)
// the Shadow shader's projection pass: colour = lerp( 1, shadowColor, texture alpha - vertex alpha )
// skinning: up to 53 bone-to-world 3x4 matrices (rows); bone 0 also sets the model matrix
enum { DX11_MAX_BONES = 53 };
void Dx11R_SetBoneMatrices( const float ( *pBones )[12], int nCount );
// how a pass uses vertex colour: Source's generic shaders ignore it unless the material
// has $vertexcolor / $vertexalpha; pColor is the material's $color and $alpha
void Dx11R_SetVertexColorUse( bool bColor, bool bAlpha, const float *pColor );
// the Water shader: refraction (stage 0) and reflection (stage 2) render targets looked up
// in screen space, tinted and mixed; pTints = refract tint, reflect tint (linear rgb + amount)
void Dx11R_SetWaterPass( bool bWater, int hReflection, const float *pRefractTint, const float *pReflectTint );
// world-space clip planes ( nx, ny, nz, d ): points with dot( n, p ) < d are clipped
void Dx11R_SetClipPlane( int nIndex, const float *pPlane, bool bEnable );
// occlusion queries: handles are 1-based; the result is the visible pixel count, -1 while pending
int  Dx11R_CreateQuery();
void Dx11R_DestroyQuery( int hQuery );
void Dx11R_BeginQuery( int hQuery );
void Dx11R_EndQuery( int hQuery );
int  Dx11R_GetQueryResult( int hQuery, bool bFlush );
// Source's LDR bloom: nMode 1 Downsample_nohdr (pParams = its c0: luminance weights, exponent),
// 2 BlurFilterX, 3 BlurFilterY (pParams[0] = bloom amount); 0 off. The size is the source texture's.
void Dx11R_SetPostPass( int nMode, const float *pParams, int nTexWidth, int nTexHeight );
// the scene's fog (linear in view depth); each pass's m_nFogMode picks what it fades to
void Dx11R_SetSceneFog( bool bEnable, float flStart, float flEnd, float flMaxDensity, const unsigned char *pColor );
// $selfillum: nMask 0 off, 1 base alpha, 2 hMask's red ($selfillummask); pTint = $selfillumtint
void Dx11R_SetSelfIllum( int nMask, const float *pTint, int hMask );
// static props: the vertex colour (from the colour mesh) is baked light, added to the dynamic lights
void Dx11R_SetStaticLight( bool bStatic );
void Dx11R_SetEnvmapParams( float flContrast, float flSaturation );
// the character shaders, from the vertex constants they set (VERTEX_SHADER_SHADER_SPECIFIC_CONST_0..5):
// nMode 1 Eyes (eye origin, half eyeball up, iris U, iris V, glint U, glint V; hIris and hGlint
// are its stages 1 and 2), 2 EyeRefract (eye origin, -, iris U, iris V), 3 Teeth (forward, w = $illumfactor);
// 0 off. Call after SetEnvmap / SetSelfIllum: the eyes reuse their texture slots.
void Dx11R_SetCharacterPass( int nMode, const float ( *pConsts )[4], float flGlintDamping, int hIris, int hGlint );
void Dx11R_SetShadowPass( bool bShadow, const float *pShadowColor );
void Dx11R_SetLighting( bool bLit, const float ambientCube[6][3], int nLights, const Dx11Light_t *pLights );

void Dx11R_SetState( const Dx11State_t &state );
void Dx11R_SetMatrices( const float *pModel, const float *pView, const float *pProj );	// row-vector 4x4
void Dx11R_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ );
void Dx11R_ClearDepth();
// render targets: hColor <= 0 is the back buffer
void Dx11R_SetRenderTarget( int hColor );
void Dx11R_Clear( bool bColor, bool bDepth );
void Dx11R_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy );
void Dx11R_GetTextureSize( int hTexture, int &nWidth, int &nHeight );
bool Dx11R_IsRenderTargetTexture( int hTexture );
// pCacheKey/nVersion: a static mesh's vertices are kept on the GPU and reused while the
// version matches (pVerts may then be NULL); pass NULL for data used once
void Dx11R_Draw( Dx11Primitive_t type, const Dx11Vertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices,
	const void *pCacheKey = 0, int nVersion = 0 );
bool Dx11R_HasCachedVertices( const void *pCacheKey, int nVersion );
void Dx11R_ReleaseCachedVertices( const void *pCacheKey );

#endif // DX11RENDER_H
