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
	float m_TangentS[3];	// model space tangent frame (bump maps)
	float m_TangentT[3];
	float m_LightmapOffset;	// bumped lightmaps: u offset between the lightmap pages, 0 if not bumped
};

// a pass's surface: the generic shaders' bump map, detail texture, blend texture, texture
// transform and phong, and water's normal map (see Dx12R_SetSurface)
struct Dx12R_Surface_t
{
	int m_nBump;				// 0 off, 1 world (bumped lightmaps), 2 model (per pixel lights), 3 water
	bool m_bSSBump;				// $ssbump: the normal map holds the three lightmaps' weights
	int m_hBump;				// t4
	int m_nDetail;				// $detailblendmode + 1, 0 off
	int m_hDetail;				// t5
	float m_flDetailScale[2];
	float m_flDetailFactor;
	int m_hBase2;				// t6: WorldVertexTransition's $basetexture2, blended by vertex alpha
	float m_BaseTransform[2][4];	// u and v rows applied to ( u, v, 0, 1 )
	float m_flWater[2];			// water: $refractamount, $reflectamount
	float m_Phong[4];			// $phongexponent (0 off), $phongboost, 1: base alpha masks it (else normal alpha)
	float m_PhongFresnel[4];	// $phongfresnelranges
	float m_PhongTint[4];
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
	int m_nFogMode;			// ShaderFogMode_t
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
// envmap reflection for this pass (the cube bound on stage 1 or 2): tint rgb; nMask 0 none, 1 base alpha,
// 2 hMask's rgb ($envmapmask), 3 hMask's alpha (the normal map's: $normalmapalphaenvmapmask)
void Dx12R_SetEnvmap( bool bEnable, const float *pTint, int nMask, const float *pEyePos, int hMask = 0 );
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
// Source's LDR bloom: nMode 1 Downsample_nohdr (pParams = its c0: luminance weights, exponent),
// 2 BlurFilterX, 3 BlurFilterY (pParams[0] = bloom amount); 0 off. The size is the source texture's.
void Dx12R_SetPostPass( int nMode, const float *pParams, int nTexWidth, int nTexHeight );
// the scene's fog (linear in view depth); each pass's m_nFogMode picks what it fades to
void Dx12R_SetSceneFog( bool bEnable, float flStart, float flEnd, float flMaxDensity, const unsigned char *pColor );
// $selfillum: nMask 0 off, 1 base alpha, 2 hMask's red ($selfillummask); pTint = $selfillumtint
void Dx12R_SetSelfIllum( int nMask, const float *pTint, int hMask );
// static props: the vertex colour (from the colour mesh) is baked light, added to the dynamic lights
void Dx12R_SetStaticLight( bool bStatic );
void Dx12R_SetEnvmapParams( float flContrast, float flSaturation );
// after SetEnvmap: the cube map's texture; bSkin = the skin (phong) shader's envmap, masked by fresnel ($envmapfresnel)
void Dx12R_SetEnvmapCube( int hCube, bool bSkin, float flFresnel );
// the character shaders, from the vertex constants they set (VERTEX_SHADER_SHADER_SPECIFIC_CONST_0..5):
// nMode 1 Eyes (eye origin, half eyeball up, iris U, iris V, glint U, glint V; hIris and hGlint
// are its stages 1 and 2), 2 EyeRefract (eye origin, -, iris U, iris V), 3 Teeth (forward, w = $illumfactor);
// 0 off. Call after SetEnvmap / SetSelfIllum: the eyes reuse their texture slots.
void Dx12R_SetCharacterPass( int nMode, const float ( *pConsts )[4], float flGlintDamping, int hIris, int hGlint );
void Dx12R_SetShadowPass( bool bShadow, const float *pShadowColor );
// every pass: the surface's extra textures and parameters (a default surface turns them off)
void Dx12R_SetSurface( const Dx12R_Surface_t &surface );
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
