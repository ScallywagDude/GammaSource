//========= GammaSource ============//
//
// Native Direct3D 11 drawing for shaderapidx11. See dx11render.h.
//
// Stage 3 of the native renderers: one textured, vertex-coloured pipeline that honours the
// materials' blend, alpha test, depth and cull state. That is what the menus, the HUD and
// plain unlit surfaces need; lightmaps, normal maps and the full shader set come next.
//
//=============================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <vector>
#include "dx11device.h"
#include "dx11render.h"

#pragma comment( lib, "d3dcompiler.lib" )

#define DEVICE ( (ID3D11Device *)Dx11_Device() )
#define CONTEXT ( (ID3D11DeviceContext *)Dx11_Context() )

struct Texture_t
{
	ID3D11Texture2D *m_pTexture;
	ID3D11ShaderResourceView *m_pSRV;
	int m_nWidth, m_nHeight, m_nMips;
	bool m_bClampS, m_bClampT;
	bool m_bUsed;
	unsigned char *m_pPending[16];	// mip levels given before the device existed
	unsigned char *m_pShadow;		// see Dx11R_LockShadow
	bool m_bRenderTarget;
	bool m_bCube;
	ID3D11RenderTargetView *m_pRTV;
	ID3D11Texture2D *m_pDepth;
	ID3D11DepthStencilView *m_pDSV;
};
static int s_hRenderTarget;	// 0: the back buffer
static std::vector< Texture_t > s_Textures;
static std::vector< ID3D11Query * > s_Queries;	// occlusion queries, handle = index + 1

struct CachedVertices_t
{
	const void *m_pKey;
	int m_nVersion;
	int m_nVerts;
	ID3D11Buffer *m_pBuffer;
};
static std::vector< CachedVertices_t > s_VertexCache;
	// handle = index + 1

static ID3D11VertexShader *s_pVS;
static ID3D11PixelShader *s_pPS;
static ID3D11InputLayout *s_pLayout;
static ID3D11Buffer *s_pConstants;
static ID3D11Buffer *s_pBoneConstants;
static float s_Bones[DX11_MAX_BONES][12];
static bool s_bBonesDirty = true;
static float s_VP[16];
static ID3D11Buffer *s_pVB, *s_pIB;
static int s_nVBSize, s_nIBSize;
static ID3D11SamplerState *s_pSamplers[4];	// bit 0 clamp S, bit 1 clamp T
static ID3D11BlendState *s_pBlendStates[128 * 4];	// blend key * 4 + write mask bits
static ID3D11DepthStencilState *s_pDepthStates[32];
static ID3D11RasterizerState *s_pRasterStates[4];	// bit 0 cull, bit 1 decal depth offset
static int s_hWhite;
static int s_hBound;
static Dx11State_t s_State;
static float s_WVP[16];

struct Constants_t
{
	float m_WVP[16];
	float m_AlphaRef;
	float m_AlphaTest;
	float m_Lightmap;
	float m_Lit;
	float m_Model[16];
	float m_Ambient[6][4];
	float m_LightColor[4][4];	// w = type
	float m_LightPos[4][4];		// w = range
	float m_LightDir[4][4];
	float m_LightAtten[4][4];	// w = spot falloff exponent
	float m_LightCone[4][4];	// x = cos inner, y = cos outer
	float m_ShadowColor[4];
	float m_Shadow;
	float m_Skin;			// bone weights per vertex, 0 = not skinned
	float m_Pad2[2];
	float m_VColorUse[4];		// same order as the HLSL cbuffer
	float m_MatColor[4];
	float m_VP[16];
	float m_Water[4];			// x on, yz 1 / target size
	float m_RefractTint[4];
	float m_ReflectTint[4];
	float m_Clip[2][4];			// disabled planes are ( 0, 0, 0, -1 ): never clip
	float m_EnvTint[4];			// w = 1: base alpha masks the envmap
	float m_EyePos[4];			// w = 1: envmap on
};

static const char s_szShader[] =
	"cbuffer Constants : register( b0 ) { row_major float4x4 g_WVP; float g_AlphaRef; float g_AlphaTest; float g_Lightmap; float g_Lit;\n"
	"  row_major float4x4 g_Model; float4 g_Ambient[6]; float4 g_LightColor[4]; float4 g_LightPos[4]; float4 g_LightDir[4];\n"
	"  float4 g_LightAtten[4]; float4 g_LightCone[4]; float4 g_ShadowColor; float g_Shadow; float g_Skin; float2 g_Pad2;\n"
	"  float4 g_VColorUse; float4 g_MatColor;\n"
	"  row_major float4x4 g_VP; float4 g_Water; float4 g_RefractTint; float4 g_ReflectTint; float4 g_Clip[2]; float4 g_EnvTint; float4 g_EyePos; };\n"
	"TextureCube g_Envmap : register( t2 ); SamplerState g_EnvSampler : register( s2 );\n"
	"cbuffer Bones : register( b1 ) { float4 g_Bones[53 * 3]; };\n"
	"Texture2D g_Texture : register( t0 ); SamplerState g_Sampler : register( s0 );\n"
	"Texture2D g_LightmapTex : register( t1 ); SamplerState g_LightmapSampler : register( s1 );\n"
	"struct VSIn { float3 pos : POSITION; float4 color : COLOR; float2 uv : TEXCOORD0; float2 uv2 : TEXCOORD1; float3 normal : NORMAL;\n"
	"  uint4 bones : BLENDINDICES; float2 weights : BLENDWEIGHT; };\n"
	"float3 SkinPos( int b, float3 p ) { return float3( dot( g_Bones[b * 3], float4( p, 1 ) ), dot( g_Bones[b * 3 + 1], float4( p, 1 ) ), dot( g_Bones[b * 3 + 2], float4( p, 1 ) ) ); }\n"
	"float3 SkinDir( int b, float3 d ) { return float3( dot( g_Bones[b * 3].xyz, d ), dot( g_Bones[b * 3 + 1].xyz, d ), dot( g_Bones[b * 3 + 2].xyz, d ) ); }\n"
	"struct PSIn { float4 pos : SV_Position; float4 color : COLOR; float2 uv : TEXCOORD0; float2 uv2 : TEXCOORD1; float2 clip : SV_ClipDistance0;\n"
	"  float3 wpos : TEXCOORD2; float3 wnorm : TEXCOORD3; };\n"
	"float3 AmbientCube( float3 n ) { float3 s = n * n; int3 i = n < 0.0f ? int3( 1, 3, 5 ) : int3( 0, 2, 4 );\n"
	"  return s.x * g_Ambient[i.x].rgb + s.y * g_Ambient[i.y].rgb + s.z * g_Ambient[i.z].rgb; }\n"
	"float3 Lighting( float3 p, float3 n ) { float3 l = AmbientCube( n );\n"
	"  [unroll] for ( int k = 0; k < 4; ++k ) { float type = g_LightColor[k].w; if ( type < 0.5f ) continue;\n"
	"    float3 dir = -g_LightDir[k].xyz; float att = 1.0f;\n"
	"    if ( type < 1.5f || type > 2.5f ) { float3 d = g_LightPos[k].xyz - p; float dist = length( d ); dir = d / max( dist, 0.001f );\n"
	"      att = 1.0f / max( g_LightAtten[k].x + g_LightAtten[k].y * dist + g_LightAtten[k].z * dist * dist, 0.0001f );\n"
	"      if ( g_LightPos[k].w > 0.0f && dist > g_LightPos[k].w ) att = 0.0f;\n"
	"      if ( type > 2.5f ) { float c = dot( -dir, g_LightDir[k].xyz );\n"
	"        float cone = saturate( ( c - g_LightCone[k].y ) / max( g_LightCone[k].x - g_LightCone[k].y, 0.0001f ) );\n"
	"        att *= pow( cone, max( g_LightAtten[k].w, 0.0001f ) ); } }\n"
	"    l += g_LightColor[k].rgb * att * saturate( dot( n, dir ) ); }\n"
	"  return l; }\n"
	"PSIn VSMain( VSIn v ) { PSIn o; o.color = lerp( 1.0f, v.color, g_VColorUse.xxxy ) * g_MatColor; o.uv = v.uv; o.uv2 = v.uv2;\n"
	"  float3 p, n;\n"
	"  if ( g_Skin > 0.5f ) { float3 w = float3( v.weights.x, g_Skin > 1.5f ? v.weights.y : 0.0f, 0.0f ); w.z = g_Skin > 2.5f ? 1.0f - w.x - w.y : 0.0f;\n"
	"    if ( g_Skin < 1.5f ) w.x = 1.0f; else if ( g_Skin < 2.5f ) w.y = 1.0f - w.x;\n"
	"    p = SkinPos( v.bones.x, v.pos ) * w.x + SkinPos( v.bones.y, v.pos ) * w.y + SkinPos( v.bones.z, v.pos ) * w.z;\n"
	"    n = SkinDir( v.bones.x, v.normal ) * w.x + SkinDir( v.bones.y, v.normal ) * w.y + SkinDir( v.bones.z, v.normal ) * w.z;\n"
	"    o.pos = mul( float4( p, 1.0f ), g_VP ); }\n"
	"  else { o.pos = mul( float4( v.pos, 1.0f ), g_WVP ); p = mul( float4( v.pos, 1.0f ), g_Model ).xyz; n = mul( v.normal, (float3x3)g_Model ); }\n"
	"  o.wpos = p; o.wnorm = n;\n"
	"  o.clip = float2( dot( g_Clip[0].xyz, p ) - g_Clip[0].w, dot( g_Clip[1].xyz, p ) - g_Clip[1].w );\n"
	"  if ( g_Lit > 0.5f ) { n = normalize( n + 1e-6f );\n"
	"    o.color.rgb *= pow( saturate( Lighting( p, n ) ), 1.0f / 2.2f ); }\n"
	"  return o; }\n"
	"float4 PSMain( PSIn i ) : SV_Target { float4 c = g_Texture.Sample( g_Sampler, i.uv ) * i.color;\n"
	"  if ( g_Lightmap > 0.5f ) c.rgb *= g_LightmapTex.Sample( g_LightmapSampler, i.uv2 ).rgb * 2.0f;\n"
	"  if ( g_EyePos.w > 0.5f ) { float3 v = normalize( i.wpos - g_EyePos.xyz ); float3 rv = reflect( v, normalize( i.wnorm + 1e-6f ) );\n"
	"    float mask = g_EnvTint.w > 0.5f ? g_Texture.Sample( g_Sampler, i.uv ).a : 1.0f;\n"
	"    c.rgb += g_Envmap.Sample( g_EnvSampler, rv ).rgb * g_EnvTint.rgb * mask; }\n"
	"  if ( g_Water.x > 0.5f ) { float2 s = i.pos.xy * g_Water.yz;\n"
	"    float3 refr = g_Texture.Sample( g_Sampler, s ).rgb * g_RefractTint.rgb;\n"
	"    float3 refl = g_LightmapTex.Sample( g_LightmapSampler, float2( s.x, 1.0f - s.y ) ).rgb * g_ReflectTint.rgb;\n"
	"    return float4( lerp( refr, refl, 0.5f ), 1.0f ); }\n"
	"  if ( g_Shadow > 0.5f ) { float cover = saturate( g_Texture.Sample( g_Sampler, i.uv ).a - i.color.a );\n"
	"    return float4( 1.0f + cover * ( g_ShadowColor.rgb - 1.0f ), 1.0f ); }\n"
	"  if ( g_AlphaTest > 0.5f ) clip( c.a - g_AlphaRef ); return c; }\n";

static ID3DBlob *Compile( const char *pszEntry, const char *pszTarget )
{
	ID3DBlob *pCode = NULL, *pErrors = NULL;
	HRESULT hr = D3DCompile( s_szShader, sizeof( s_szShader ) - 1, "gammasource_dx11", NULL, NULL, pszEntry, pszTarget,
		D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCode, &pErrors );
	if ( pErrors )
	{
		OutputDebugStringA( (const char *)pErrors->GetBufferPointer() );
		pErrors->Release();
	}
	return SUCCEEDED( hr ) ? pCode : NULL;
}

bool Dx11R_Init()
{
	if ( s_pVS )
		return true;
	ID3D11Device *pDevice = DEVICE;
	if ( !pDevice )
		return false;
	ID3DBlob *pVS = Compile( "VSMain", "vs_4_0" );
	ID3DBlob *pPS = Compile( "PSMain", "ps_4_0" );
	if ( !pVS || !pPS )
		return false;
	pDevice->CreateVertexShader( pVS->GetBufferPointer(), pVS->GetBufferSize(), NULL, &s_pVS );
	pDevice->CreatePixelShader( pPS->GetBufferPointer(), pPS->GetBufferSize(), NULL, &s_pPS );
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	pDevice->CreateInputLayout( layout, 7, pVS->GetBufferPointer(), pVS->GetBufferSize(), &s_pLayout );
	pVS->Release(); pPS->Release();

	D3D11_BUFFER_DESC cb = {};
	cb.ByteWidth = sizeof( Constants_t );
	cb.Usage = D3D11_USAGE_DYNAMIC;
	cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	pDevice->CreateBuffer( &cb, NULL, &s_pConstants );
	cb.ByteWidth = sizeof( s_Bones );
	pDevice->CreateBuffer( &cb, NULL, &s_pBoneConstants );

	for ( int i = 0; i < 4; ++i )
	{
		D3D11_SAMPLER_DESC sd = {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = ( i & 1 ) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressV = ( i & 2 ) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		pDevice->CreateSamplerState( &sd, &s_pSamplers[i] );
	}
	for ( int i = 0; i < 4; ++i )
	{
		D3D11_RASTERIZER_DESC rd = {};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = ( i & 1 ) ? D3D11_CULL_BACK : D3D11_CULL_NONE;
		if ( i & 2 )
		{
			// decals and overlays sit on their surface: pull them forward like D3D9's decal bias
			// (mat_depthbias_decal -0.000005 on a 24 bit buffer, mat_slopescaledepthbias_decal -0.5)
			rd.DepthBias = -84;
			rd.SlopeScaledDepthBias = -0.5f;
		}
		rd.FrontCounterClockwise = FALSE;	// as D3D9: front faces are clockwise
		rd.DepthClipEnable = TRUE;
		rd.ScissorEnable = FALSE;
		pDevice->CreateRasterizerState( &rd, &s_pRasterStates[i] );
	}

	static const unsigned char s_White[4] = { 255, 255, 255, 255 };
	s_hWhite = Dx11R_CreateTexture( 1, 1, 1 );
	Dx11R_TexImage( s_hWhite, 0, 1, 1, s_White );

	memset( &s_State, 0, sizeof( s_State ) );
	s_State.m_bDepthTest = true;
	s_State.m_bDepthWrite = true;
	s_State.m_nDepthFunc = 3;	// near or equal
	for ( int i = 0; i < 16; ++i )
		s_WVP[i] = ( i % 5 ) ? 0.0f : 1.0f;
	return true;
}

void Dx11R_Shutdown()
{
#define SAFE_RELEASE( p ) if ( p ) { p->Release(); p = NULL; }
	for ( size_t i = 0; i < s_Textures.size(); ++i )
	{
		SAFE_RELEASE( s_Textures[i].m_pSRV );
		SAFE_RELEASE( s_Textures[i].m_pTexture );
		SAFE_RELEASE( s_Textures[i].m_pRTV );
		SAFE_RELEASE( s_Textures[i].m_pDSV );
		SAFE_RELEASE( s_Textures[i].m_pDepth );
	}
	s_Textures.clear();
	for ( int i = 0; i < 4; ++i ) SAFE_RELEASE( s_pSamplers[i] );
	for ( int i = 0; i < 128 * 4; ++i ) SAFE_RELEASE( s_pBlendStates[i] );
	for ( int i = 0; i < 32; ++i ) SAFE_RELEASE( s_pDepthStates[i] );
	for ( int i = 0; i < 4; ++i ) SAFE_RELEASE( s_pRasterStates[i] );
	for ( size_t i = 0; i < s_Queries.size(); ++i )
		SAFE_RELEASE( s_Queries[i] );
	s_Queries.clear();
	SAFE_RELEASE( s_pVB ); SAFE_RELEASE( s_pIB ); SAFE_RELEASE( s_pConstants ); SAFE_RELEASE( s_pBoneConstants );
	for ( size_t i = 0; i < s_VertexCache.size(); ++i )
		SAFE_RELEASE( s_VertexCache[i].m_pBuffer );
	s_VertexCache.clear();
	SAFE_RELEASE( s_pLayout ); SAFE_RELEASE( s_pVS ); SAFE_RELEASE( s_pPS );
	s_nVBSize = s_nIBSize = 0;
	s_hWhite = s_hBound = 0;
}

//-----------------------------------------------------------------------------
// Textures
//-----------------------------------------------------------------------------
static Texture_t *GetTexture( int hTexture )
{
	if ( hTexture <= 0 || hTexture > (int)s_Textures.size() || !s_Textures[hTexture - 1].m_bUsed )
		return NULL;
	return &s_Textures[hTexture - 1];
}

int Dx11R_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget, bool bCube )
{
	Texture_t tex = {};
	tex.m_nWidth = nWidth > 0 ? nWidth : 1;
	tex.m_nHeight = nHeight > 0 ? nHeight : 1;
	tex.m_nMips = ( nMips > 0 && !bRenderTarget ) ? nMips : 1;
	tex.m_bRenderTarget = bRenderTarget;
	tex.m_bCube = bCube && !bRenderTarget;
	tex.m_bUsed = true;
	for ( size_t i = 0; i < s_Textures.size(); ++i )
	{
		if ( !s_Textures[i].m_bUsed )
		{
			s_Textures[i] = tex;
			return (int)i + 1;
		}
	}
	s_Textures.push_back( tex );
	return (int)s_Textures.size();
}

void Dx11R_DeleteTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return;
	if ( hTexture == s_hRenderTarget )
		Dx11R_SetRenderTarget( 0 );
	if ( pTex->m_pSRV ) pTex->m_pSRV->Release();
	if ( pTex->m_pTexture ) pTex->m_pTexture->Release();
	if ( pTex->m_pRTV ) pTex->m_pRTV->Release();
	if ( pTex->m_pDSV ) pTex->m_pDSV->Release();
	if ( pTex->m_pDepth ) pTex->m_pDepth->Release();
	for ( int i = 0; i < 16; ++i ) free( pTex->m_pPending[i] );
	free( pTex->m_pShadow );
	memset( pTex, 0, sizeof( *pTex ) );
}

bool Dx11R_IsTexture( int hTexture )
{
	return GetTexture( hTexture ) != NULL;
}

static int MipSize( int n, int nLevel )
{
	n >>= nLevel;
	return n > 0 ? n : 1;
}

static bool EnsureResource( Texture_t *pTex )
{
	if ( pTex->m_pTexture )
		return true;
	if ( !DEVICE )
		return false;
	D3D11_TEXTURE2D_DESC td = {};
	td.Width = pTex->m_nWidth;
	td.Height = pTex->m_nHeight;
	td.MipLevels = pTex->m_nMips;
	td.ArraySize = pTex->m_bCube ? 6 : 1;
	td.MiscFlags = pTex->m_bCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
	td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE | ( pTex->m_bRenderTarget ? D3D11_BIND_RENDER_TARGET : 0 );
	if ( FAILED( DEVICE->CreateTexture2D( &td, NULL, &pTex->m_pTexture ) ) )
		return false;
	if ( pTex->m_bRenderTarget && FAILED( DEVICE->CreateRenderTargetView( pTex->m_pTexture, NULL, &pTex->m_pRTV ) ) )
		return false;
	D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
	svd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	if ( pTex->m_bCube )
	{
		svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		svd.TextureCube.MipLevels = pTex->m_nMips;
	}
	else
	{
		svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		svd.Texture2D.MipLevels = pTex->m_nMips;
	}
	if ( FAILED( DEVICE->CreateShaderResourceView( pTex->m_pTexture, &svd, &pTex->m_pSRV ) ) )
		return false;
	// data that arrived before the device existed
	for ( int i = 0; i < 16 && i < pTex->m_nMips; ++i )
	{
		if ( !pTex->m_pPending[i] )
			continue;
		CONTEXT->UpdateSubresource( pTex->m_pTexture, i, NULL, pTex->m_pPending[i], MipSize( pTex->m_nWidth, i ) * 4, 0 );
		free( pTex->m_pPending[i] );
		pTex->m_pPending[i] = NULL;
	}
	return true;
}

static void KeepPending( Texture_t *pTex, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	if ( nLevel >= 16 )
		return;
	int nLevelW = MipSize( pTex->m_nWidth, nLevel ), nLevelH = MipSize( pTex->m_nHeight, nLevel );
	if ( !pTex->m_pPending[nLevel] )
		pTex->m_pPending[nLevel] = (unsigned char *)calloc( nLevelW * nLevelH, 4 );
	for ( int row = 0; row < nHeight && y + row < nLevelH; ++row )
	{
		int nCopy = nWidth;
		if ( x + nCopy > nLevelW ) nCopy = nLevelW - x;
		if ( nCopy > 0 )
			memcpy( pTex->m_pPending[nLevel] + ( ( y + row ) * nLevelW + x ) * 4, pBGRA + row * nWidth * 4, nCopy * 4 );
	}
}

// one face of a cube texture (cubes only get data once the device exists)
void Dx11R_TexImageFace( int hTexture, int nFace, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || !pTex->m_bCube || nFace < 0 || nFace >= 6 || nLevel >= pTex->m_nMips || !EnsureResource( pTex ) )
		return;
	CONTEXT->UpdateSubresource( pTex->m_pTexture, D3D11CalcSubresource( nLevel, nFace, pTex->m_nMips ), NULL, pBGRA, nWidth * 4, 0 );
}

bool Dx11R_IsCubeTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	return pTex && pTex->m_bCube;
}

void Dx11R_TexImage( int hTexture, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	extern int g_nDx11Uploads;
	++g_nDx11Uploads;
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nLevel >= pTex->m_nMips )
		return;
	if ( nLevel == 0 && ( nWidth != pTex->m_nWidth || nHeight != pTex->m_nHeight ) && !pTex->m_pTexture )
	{
		pTex->m_nWidth = nWidth;
		pTex->m_nHeight = nHeight;
	}
	if ( !EnsureResource( pTex ) )
	{
		KeepPending( pTex, nLevel, 0, 0, nWidth, nHeight, pBGRA );
		return;
	}
	CONTEXT->UpdateSubresource( pTex->m_pTexture, nLevel, NULL, pBGRA, nWidth * 4, 0 );
}

void Dx11R_TexSubImage( int hTexture, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nLevel >= pTex->m_nMips )
		return;
	if ( !EnsureResource( pTex ) )
	{
		KeepPending( pTex, nLevel, x, y, nWidth, nHeight, pBGRA );
		return;
	}
	D3D11_BOX box = { (UINT)x, (UINT)y, 0, (UINT)( x + nWidth ), (UINT)( y + nHeight ), 1 };
	CONTEXT->UpdateSubresource( pTex->m_pTexture, nLevel, &box, pBGRA, nWidth * 4, 0 );
}

unsigned char *Dx11R_LockShadow( int hTexture, int &nPitch, int &nWidth, int &nHeight )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return NULL;
	if ( !pTex->m_pShadow )
		pTex->m_pShadow = (unsigned char *)calloc( pTex->m_nWidth * pTex->m_nHeight, 4 );
	nPitch = pTex->m_nWidth * 4;
	nWidth = pTex->m_nWidth;
	nHeight = pTex->m_nHeight;
	return pTex->m_pShadow;
}

void Dx11R_UploadRect( int hTexture, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nWidth <= 0 || nHeight <= 0 )
		return;
	if ( !EnsureResource( pTex ) )
	{
		for ( int row = 0; row < nHeight; ++row )
			KeepPending( pTex, 0, x, y + row, nWidth, 1, pBGRA + row * nPitch );
		return;
	}
	D3D11_BOX box = { (UINT)x, (UINT)y, 0, (UINT)( x + nWidth ), (UINT)( y + nHeight ), 1 };
	CONTEXT->UpdateSubresource( pTex->m_pTexture, 0, &box, pBGRA, nPitch, 0 );
}

void Dx11R_TexWrap( int hTexture, int nCoord, bool bClamp )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return;
	if ( nCoord == 0 )
		pTex->m_bClampS = bClamp;
	else if ( nCoord == 1 )
		pTex->m_bClampT = bClamp;
}

static int s_hLightmap;
static int s_hStage2;
static int s_hStage1;
static bool s_bEnvmap;
static float s_EnvTint[4], s_EyePos[4];

void Dx11R_SetEnvmap( bool bEnable, const float *pTint, int nMask, const float *pEyePos )
{
	s_bEnvmap = bEnable;
	if ( !bEnable )
		return;
	for ( int i = 0; i < 3; ++i )
	{
		s_EnvTint[i] = pTint ? pTint[i] : 1.0f;
		s_EyePos[i] = pEyePos ? pEyePos[i] : 0.0f;
	}
	s_EnvTint[3] = (float)nMask;
	s_EyePos[3] = 1.0f;
}
static bool s_bLit;
static bool s_bShadowPass;

int Dx11R_CreateQuery()
{
	if ( !DEVICE )
		return 0;
	D3D11_QUERY_DESC qd = { D3D11_QUERY_OCCLUSION, 0 };
	ID3D11Query *pQuery = NULL;
	if ( FAILED( DEVICE->CreateQuery( &qd, &pQuery ) ) )
		return 0;
	for ( size_t i = 0; i < s_Queries.size(); ++i )
	{
		if ( !s_Queries[i] )
		{
			s_Queries[i] = pQuery;
			return (int)i + 1;
		}
	}
	s_Queries.push_back( pQuery );
	return (int)s_Queries.size();
}

static ID3D11Query *GetQuery( int hQuery )
{
	return ( hQuery > 0 && hQuery <= (int)s_Queries.size() ) ? s_Queries[hQuery - 1] : NULL;
}

void Dx11R_DestroyQuery( int hQuery )
{
	ID3D11Query *pQuery = GetQuery( hQuery );
	if ( pQuery )
	{
		pQuery->Release();
		s_Queries[hQuery - 1] = NULL;
	}
}

void Dx11R_BeginQuery( int hQuery )
{
	ID3D11Query *pQuery = GetQuery( hQuery );
	if ( pQuery && CONTEXT ) CONTEXT->Begin( pQuery );
}

void Dx11R_EndQuery( int hQuery )
{
	ID3D11Query *pQuery = GetQuery( hQuery );
	if ( pQuery && CONTEXT ) CONTEXT->End( pQuery );
}

int Dx11R_GetQueryResult( int hQuery, bool bFlush )
{
	ID3D11Query *pQuery = GetQuery( hQuery );
	if ( !pQuery || !CONTEXT )
		return -2;	// OCCLUSION_QUERY_RESULT_ERROR
	UINT64 nPixels = 0;
	HRESULT hr = CONTEXT->GetData( pQuery, &nPixels, sizeof( nPixels ), bFlush ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH );
	if ( hr != S_OK )
		return -1;	// OCCLUSION_QUERY_RESULT_PENDING
	return nPixels > 0x7fffffff ? 0x7fffffff : (int)nPixels;
}
static bool s_bWaterPass;
static float s_ClipPlanes[2][4] = { { 0, 0, 0, -1 }, { 0, 0, 0, -1 } };

void Dx11R_SetClipPlane( int nIndex, const float *pPlane, bool bEnable )
{
	if ( nIndex < 0 || nIndex >= 2 )
		return;
	static const float s_Off[4] = { 0, 0, 0, -1 };
	memcpy( s_ClipPlanes[nIndex], ( bEnable && pPlane ) ? pPlane : s_Off, sizeof( s_Off ) );
}
static int s_hWaterReflection;
static float s_RefractTint[4] = { 1, 1, 1, 1 }, s_ReflectTint[4] = { 1, 1, 1, 1 };

void Dx11R_SetWaterPass( bool bWater, int hReflection, const float *pRefractTint, const float *pReflectTint )
{
	s_bWaterPass = bWater;
	s_hWaterReflection = hReflection;
	for ( int i = 0; i < 4; ++i )
	{
		// the constants are linear; this shader works in gamma space
		s_RefractTint[i] = pRefractTint ? powf( pRefractTint[i] > 0.0f ? pRefractTint[i] : 0.0f, 1.0f / 2.2f ) : 1.0f;
		s_ReflectTint[i] = pReflectTint ? powf( pReflectTint[i] > 0.0f ? pReflectTint[i] : 0.0f, 1.0f / 2.2f ) : 1.0f;
	}
}
static float s_VColorUse[4] = { 1, 1, 0, 0 };
static float s_MatColor[4] = { 1, 1, 1, 1 };

void Dx11R_SetVertexColorUse( bool bColor, bool bAlpha, const float *pColor )
{
	s_VColorUse[0] = bColor ? 1.0f : 0.0f;
	s_VColorUse[1] = bAlpha ? 1.0f : 0.0f;
	for ( int i = 0; i < 4; ++i )
		s_MatColor[i] = pColor ? pColor[i] : 1.0f;
}

void Dx11R_SetBoneMatrices( const float ( *pBones )[12], int nCount )
{
	if ( nCount > DX11_MAX_BONES ) nCount = DX11_MAX_BONES;
	if ( nCount > 0 && memcmp( s_Bones, pBones, nCount * sizeof( s_Bones[0] ) ) )
	{
		memcpy( s_Bones, pBones, nCount * sizeof( s_Bones[0] ) );
		s_bBonesDirty = true;
	}
}
static float s_ShadowColor[4];

void Dx11R_SetShadowPass( bool bShadow, const float *pShadowColor )
{
	s_bShadowPass = bShadow;
	if ( bShadow && pShadowColor )
		memcpy( s_ShadowColor, pShadowColor, sizeof( s_ShadowColor ) );
}
static float s_Model[16];
static float s_Ambient[6][3];
static int s_nLights;
static Dx11Light_t s_Lights[4];

void Dx11R_SetLighting( bool bLit, const float ambientCube[6][3], int nLights, const Dx11Light_t *pLights )
{
	static int s_nNoLit = -1;
	if ( s_nNoLit < 0 ) { char b[8]; s_nNoLit = GetEnvironmentVariableA( "GAMMA_DX11_NOLIT", b, 8 ) ? 1 : 0; }
	s_bLit = bLit && !s_nNoLit;
	if ( !s_bLit )
		return;
	memcpy( s_Ambient, ambientCube, sizeof( s_Ambient ) );
	s_nLights = nLights < 4 ? nLights : 4;
	for ( int i = 0; i < s_nLights; ++i )
		s_Lights[i] = pLights[i];
}

void Dx11R_BindLightmap( int hTexture )
{
	s_hLightmap = hTexture;
}

void Dx11R_BindTexture( int nStage, int hTexture )
{
	extern int g_nDx11Binds;
	if ( hTexture > 0 ) ++g_nDx11Binds;
	if ( nStage == 0 )
		s_hBound = hTexture;
	else if ( nStage == 1 )
		s_hStage1 = hTexture;
	else if ( nStage == 2 )
		s_hStage2 = hTexture;
}

int Dx11R_GetBoundTexture( int nStage )
{
	return nStage == 0 ? s_hBound : nStage == 2 ? s_hStage2 : 0;
}

//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------
void Dx11R_SetState( const Dx11State_t &state )
{
	s_State = state;
}

static void Mul( const float *a, const float *b, float *out )
{
	float r[16];
	for ( int i = 0; i < 4; ++i )
		for ( int j = 0; j < 4; ++j )
			r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] + a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
	memcpy( out, r, sizeof( r ) );
}

void Dx11R_SetMatrices( const float *pModel, const float *pView, const float *pProj )
{
	float mv[16];
	memcpy( s_Model, pModel, sizeof( s_Model ) );
	Mul( pModel, pView, mv );
	Mul( mv, pProj, s_WVP );
	Mul( pView, pProj, s_VP );
}

// kept here and applied per draw: the engine can set it before the device exists
static D3D11_VIEWPORT s_Viewport;

void Dx11R_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ )
{
	D3D11_VIEWPORT vp = { (float)x, (float)y, (float)w, (float)h, flMinZ, flMaxZ };
	s_Viewport = vp;
}

int g_nDx11Draws, g_nDx11Meshes, g_nDx11Passes, g_nDx11EmptyMeshes, g_nDx11Binds, g_nDx11Textured, g_nDx11Uploads;

void Dx11R_ClearDepth()
{
	Dx11R_Clear( false, true );
}

bool Dx11R_IsRenderTargetTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	return pTex && pTex->m_bRenderTarget;
}

void Dx11R_GetTextureSize( int hTexture, int &nWidth, int &nHeight )
{
	Texture_t *pTex = GetTexture( hTexture );
	nWidth = pTex ? pTex->m_nWidth : 0;
	nHeight = pTex ? pTex->m_nHeight : 0;
}

// depth for a render target: the back buffer's when the sizes match, else its own
static ID3D11DepthStencilView *TargetDepth( Texture_t *pTex )
{
	int nBackW, nBackH;
	Dx11_GetBackBufferSize( nBackW, nBackH );
	if ( pTex->m_nWidth == nBackW && pTex->m_nHeight == nBackH )
		return (ID3D11DepthStencilView *)Dx11_BackBufferDSV();
	if ( !pTex->m_pDSV )
	{
		D3D11_TEXTURE2D_DESC dd = {};
		dd.Width = pTex->m_nWidth; dd.Height = pTex->m_nHeight; dd.MipLevels = 1; dd.ArraySize = 1;
		dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; dd.SampleDesc.Count = 1;
		dd.Usage = D3D11_USAGE_DEFAULT; dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		if ( SUCCEEDED( DEVICE->CreateTexture2D( &dd, NULL, &pTex->m_pDepth ) ) )
			DEVICE->CreateDepthStencilView( pTex->m_pDepth, NULL, &pTex->m_pDSV );
	}
	return pTex->m_pDSV;
}

void Dx11R_SetRenderTarget( int hColor )
{
	Texture_t *pTex = GetTexture( hColor );
	if ( !pTex || !pTex->m_bRenderTarget || !CONTEXT || !EnsureResource( pTex ) || !pTex->m_pRTV )
	{
		s_hRenderTarget = 0;
		Dx11_BindBackBuffer();
		return;
	}
	// a texture can't be read while it is the target
	ID3D11ShaderResourceView *pNull = NULL;
	CONTEXT->PSSetShaderResources( 0, 1, &pNull );
	CONTEXT->OMSetRenderTargets( 1, &pTex->m_pRTV, TargetDepth( pTex ) );
	s_hRenderTarget = hColor;
}

void Dx11R_Clear( bool bColor, bool bDepth )
{
	if ( !CONTEXT )
		return;
	ID3D11RenderTargetView *pRTV = (ID3D11RenderTargetView *)Dx11_BackBufferRTV();
	ID3D11DepthStencilView *pDSV = (ID3D11DepthStencilView *)Dx11_BackBufferDSV();
	Texture_t *pTex = GetTexture( s_hRenderTarget );
	if ( pTex && pTex->m_pRTV )
	{
		pRTV = pTex->m_pRTV;
		pDSV = TargetDepth( pTex );
	}
	if ( bColor && pRTV )
		CONTEXT->ClearRenderTargetView( pRTV, Dx11_GetClearColor() );
	if ( bDepth && pDSV )
		CONTEXT->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
}

void Dx11R_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy )
{
	Texture_t *pDst = GetTexture( hTexture );
	if ( !pDst || !CONTEXT || !EnsureResource( pDst ) || hTexture == s_hRenderTarget )
		return;
	ID3D11Texture2D *pSrc = NULL;
	Texture_t *pSrcTex = GetTexture( s_hRenderTarget );
	if ( pSrcTex && pSrcTex->m_pTexture )
	{
		pSrc = pSrcTex->m_pTexture;
		pSrc->AddRef();
	}
	else
	{
		pSrc = (ID3D11Texture2D *)Dx11_AcquireBackBuffer();
	}
	if ( !pSrc )
		return;
	D3D11_TEXTURE2D_DESC sd;
	pSrc->GetDesc( &sd );
	// clip the copy to both surfaces (no scaling here)
	if ( sx < 0 ) { dx -= sx; sw += sx; sx = 0; }
	if ( sy < 0 ) { dy -= sy; sh += sy; sy = 0; }
	if ( dx < 0 ) { sx -= dx; sw += dx; dx = 0; }
	if ( dy < 0 ) { sy -= dy; sh += dy; dy = 0; }
	if ( sx + sw > (int)sd.Width ) sw = sd.Width - sx;
	if ( sy + sh > (int)sd.Height ) sh = sd.Height - sy;
	if ( dx + sw > pDst->m_nWidth ) sw = pDst->m_nWidth - dx;
	if ( dy + sh > pDst->m_nHeight ) sh = pDst->m_nHeight - dy;
	if ( sw > 0 && sh > 0 && sd.Format == DXGI_FORMAT_B8G8R8A8_UNORM )
	{
		D3D11_BOX box = { (UINT)sx, (UINT)sy, 0, (UINT)( sx + sw ), (UINT)( sy + sh ), 1 };
		CONTEXT->CopySubresourceRegion( pDst->m_pTexture, 0, dx, dy, 0, pSrc, 0, &box );
	}
	pSrc->Release();
}

static const D3D11_BLEND s_BlendMap[] =
{
	D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_SRC_ALPHA_SAT, D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR
};
static const D3D11_COMPARISON_FUNC s_DepthMap[] =
{
	D3D11_COMPARISON_NEVER, D3D11_COMPARISON_LESS, D3D11_COMPARISON_EQUAL, D3D11_COMPARISON_LESS_EQUAL,
	D3D11_COMPARISON_GREATER, D3D11_COMPARISON_NOT_EQUAL, D3D11_COMPARISON_GREATER_EQUAL, D3D11_COMPARISON_ALWAYS
};

static void ApplyState()
{
	ID3D11Device *pDevice = DEVICE;
	ID3D11DeviceContext *pContext = CONTEXT;

	int nSrc = ( s_State.m_nSrcBlend >= 0 && s_State.m_nSrcBlend <= 10 ) ? s_State.m_nSrcBlend : 1;
	int nDst = ( s_State.m_nDstBlend >= 0 && s_State.m_nDstBlend <= 10 ) ? s_State.m_nDstBlend : 0;
	int nBlendKey = ( s_State.m_bBlend ? ( 1 + nSrc * 11 + nDst ) % 128 : 0 ) * 4 + ( s_State.m_bNoColorWrites ? 1 : 0 ) + ( s_State.m_bNoAlphaWrites ? 2 : 0 );
	if ( !s_pBlendStates[nBlendKey] )
	{
		D3D11_BLEND_DESC bd = {};
		bd.RenderTarget[0].BlendEnable = s_State.m_bBlend;
		bd.RenderTarget[0].SrcBlend = s_BlendMap[nSrc];
		bd.RenderTarget[0].DestBlend = s_BlendMap[nDst];
		bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].RenderTargetWriteMask = ( s_State.m_bNoColorWrites ? 0 : ( D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE ) ) |
			( s_State.m_bNoAlphaWrites ? 0 : D3D11_COLOR_WRITE_ENABLE_ALPHA );
		pDevice->CreateBlendState( &bd, &s_pBlendStates[nBlendKey] );
	}
	pContext->OMSetBlendState( s_pBlendStates[nBlendKey], NULL, 0xFFFFFFFF );

	int nDepthFunc = ( s_State.m_nDepthFunc >= 0 && s_State.m_nDepthFunc <= 7 ) ? s_State.m_nDepthFunc : 3;
	int nDepthKey = ( s_State.m_bDepthTest ? 1 : 0 ) | ( s_State.m_bDepthWrite ? 2 : 0 ) | ( nDepthFunc << 2 );
	if ( !s_pDepthStates[nDepthKey] )
	{
		D3D11_DEPTH_STENCIL_DESC dd = {};
		dd.DepthEnable = s_State.m_bDepthTest || s_State.m_bDepthWrite;
		dd.DepthWriteMask = s_State.m_bDepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		dd.DepthFunc = s_State.m_bDepthTest ? s_DepthMap[nDepthFunc] : D3D11_COMPARISON_ALWAYS;
		pDevice->CreateDepthStencilState( &dd, &s_pDepthStates[nDepthKey] );
	}
	pContext->OMSetDepthStencilState( s_pDepthStates[nDepthKey], 0 );
	pContext->RSSetState( s_pRasterStates[( s_State.m_bCull ? 1 : 0 ) | ( s_State.m_bDecalOffset ? 2 : 0 )] );
	D3D11_VIEWPORT vp = s_Viewport;
	if ( vp.Width <= 0 || vp.Height <= 0 )
	{
		int w, h;
		Dx11_GetBackBufferSize( w, h );
		vp.TopLeftX = 0; vp.TopLeftY = 0; vp.Width = (float)w; vp.Height = (float)h; vp.MinDepth = 0; vp.MaxDepth = 1;
	}
	pContext->RSSetViewports( 1, &vp );

	D3D11_MAPPED_SUBRESOURCE mapped;
	if ( SUCCEEDED( pContext->Map( s_pConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) )
	{
		Constants_t *pConst = (Constants_t *)mapped.pData;
		memcpy( pConst->m_WVP, s_WVP, sizeof( s_WVP ) );
		pConst->m_AlphaRef = s_State.m_flAlphaRef;
		pConst->m_AlphaTest = s_State.m_bAlphaTest ? 1.0f : 0.0f;
		Texture_t *pLightmap = GetTexture( s_hLightmap );
		if ( pLightmap && !pLightmap->m_pSRV )
			EnsureResource( pLightmap );
		static int s_nNoLightmaps = -1;
		if ( s_nNoLightmaps < 0 ) { char b[8]; s_nNoLightmaps = GetEnvironmentVariableA( "GAMMA_DX11_NOLM", b, 8 ) ? 1 : 0; }
		pConst->m_Lightmap = ( pLightmap && pLightmap->m_pSRV && !s_nNoLightmaps ) ? 1.0f : 0.0f;
		pConst->m_Shadow = s_bShadowPass ? 1.0f : 0.0f;
		pConst->m_Skin = (float)s_State.m_nBoneWeights;
		memcpy( pConst->m_VColorUse, s_VColorUse, sizeof( s_VColorUse ) );
		pConst->m_Water[0] = s_bWaterPass ? 1.0f : 0.0f;
		{
			int nTargetW, nTargetH;
			Texture_t *pTarget = GetTexture( s_hRenderTarget );
			if ( pTarget ) { nTargetW = pTarget->m_nWidth; nTargetH = pTarget->m_nHeight; }
			else Dx11_GetBackBufferSize( nTargetW, nTargetH );
			pConst->m_Water[1] = 1.0f / ( nTargetW > 0 ? nTargetW : 1 );
			pConst->m_Water[2] = 1.0f / ( nTargetH > 0 ? nTargetH : 1 );
			pConst->m_Water[3] = 0.0f;
		}
		memcpy( pConst->m_RefractTint, s_RefractTint, sizeof( s_RefractTint ) );
		memcpy( pConst->m_ReflectTint, s_ReflectTint, sizeof( s_ReflectTint ) );
		memcpy( pConst->m_Clip, s_ClipPlanes, sizeof( s_ClipPlanes ) );
		memcpy( pConst->m_EnvTint, s_EnvTint, sizeof( s_EnvTint ) );
		memcpy( pConst->m_EyePos, s_EyePos, sizeof( s_EyePos ) );
		if ( !s_bEnvmap )
			pConst->m_EyePos[3] = 0.0f;
		memcpy( pConst->m_MatColor, s_MatColor, sizeof( s_MatColor ) );
		memcpy( pConst->m_VP, s_VP, sizeof( s_VP ) );
		for ( int i = 0; i < 3; ++i )	// the constant is linear; this shader works in gamma space
			pConst->m_ShadowColor[i] = powf( s_ShadowColor[i] > 0.0f ? s_ShadowColor[i] : 0.0f, 1.0f / 2.2f );
		pConst->m_ShadowColor[3] = 1.0f;
		bool bLit = s_bLit && s_State.m_bNormals;
		pConst->m_Lit = bLit ? 1.0f : 0.0f;
		memcpy( pConst->m_Model, s_Model, sizeof( s_Model ) );
		memset( pConst->m_Ambient, 0, sizeof( pConst->m_Ambient ) + sizeof( pConst->m_LightColor ) + sizeof( pConst->m_LightPos ) +
			sizeof( pConst->m_LightDir ) + sizeof( pConst->m_LightAtten ) + sizeof( pConst->m_LightCone ) );
		if ( bLit )
		{
			for ( int i = 0; i < 6; ++i )
				memcpy( pConst->m_Ambient[i], s_Ambient[i], sizeof( s_Ambient[i] ) );
			for ( int i = 0; i < s_nLights; ++i )
			{
				const Dx11Light_t &l = s_Lights[i];
				memcpy( pConst->m_LightColor[i], l.m_Color, 12 ); pConst->m_LightColor[i][3] = (float)l.m_nType;
				memcpy( pConst->m_LightPos[i], l.m_Pos, 12 ); pConst->m_LightPos[i][3] = l.m_flRange;
				memcpy( pConst->m_LightDir[i], l.m_Dir, 12 );
				memcpy( pConst->m_LightAtten[i], l.m_flAtten, 12 ); pConst->m_LightAtten[i][3] = l.m_flFalloff;
				pConst->m_LightCone[i][0] = l.m_flThetaDot; pConst->m_LightCone[i][1] = l.m_flPhiDot;
			}
		}
		pContext->Unmap( s_pConstants, 0 );
	}

	Texture_t *pTex = GetTexture( s_hBound );
	if ( pTex && !pTex->m_pSRV )
		EnsureResource( pTex );
	if ( !pTex || !pTex->m_pSRV )
		pTex = GetTexture( s_hWhite );
	extern int g_nDx11Textured;
	if ( pTex && pTex != GetTexture( s_hWhite ) ) ++g_nDx11Textured;
	ID3D11ShaderResourceView *pSRV = pTex ? pTex->m_pSRV : NULL;
	pContext->PSSetShaderResources( 0, 1, &pSRV );
	int nSampler = pTex ? ( ( pTex->m_bClampS ? 1 : 0 ) | ( pTex->m_bClampT ? 2 : 0 ) ) : 0;
	pContext->PSSetSamplers( 0, 1, &s_pSamplers[nSampler] );
	{
		Texture_t *pLightmap = GetTexture( s_bWaterPass ? s_hWaterReflection : s_hLightmap );
		if ( pLightmap && !pLightmap->m_pSRV ) EnsureResource( pLightmap );
		ID3D11ShaderResourceView *pLightmapSRV = pLightmap ? pLightmap->m_pSRV : NULL;
		pContext->PSSetShaderResources( 1, 1, &pLightmapSRV );
		pContext->PSSetSamplers( 1, 1, &s_pSamplers[3] );
		Texture_t *pEnvmap = NULL;
		if ( s_bEnvmap )
			pEnvmap = Dx11R_IsCubeTexture( s_hStage1 ) ? GetTexture( s_hStage1 ) : GetTexture( s_hStage2 );
		if ( pEnvmap && !pEnvmap->m_pSRV ) EnsureResource( pEnvmap );
		ID3D11ShaderResourceView *pEnvSRV = ( pEnvmap && pEnvmap->m_bCube ) ? pEnvmap->m_pSRV : NULL;
		pContext->PSSetShaderResources( 2, 1, &pEnvSRV );
		pContext->PSSetSamplers( 2, 1, &s_pSamplers[3] );
	}

	pContext->IASetInputLayout( s_pLayout );
	pContext->VSSetShader( s_pVS, NULL, 0 );
	pContext->VSSetConstantBuffers( 0, 1, &s_pConstants );
	if ( s_State.m_nBoneWeights > 0 && s_bBonesDirty && s_pBoneConstants )
	{
		D3D11_MAPPED_SUBRESOURCE bones;
		if ( SUCCEEDED( pContext->Map( s_pBoneConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &bones ) ) )
		{
			memcpy( bones.pData, s_Bones, sizeof( s_Bones ) );
			pContext->Unmap( s_pBoneConstants, 0 );
			s_bBonesDirty = false;
		}
	}
	pContext->VSSetConstantBuffers( 1, 1, &s_pBoneConstants );
	pContext->PSSetShader( s_pPS, NULL, 0 );
	pContext->PSSetConstantBuffers( 0, 1, &s_pConstants );
}

static bool Upload( ID3D11Buffer **ppBuffer, int *pnSize, UINT nBind, const void *pData, int nBytes )
{
	if ( nBytes > *pnSize )
	{
		if ( *ppBuffer ) { ( *ppBuffer )->Release(); *ppBuffer = NULL; }
		int nSize = 64 * 1024;
		while ( nSize < nBytes ) nSize *= 2;
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth = nSize;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = nBind;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if ( FAILED( DEVICE->CreateBuffer( &bd, NULL, ppBuffer ) ) )
		{
			*pnSize = 0;
			return false;
		}
		*pnSize = nSize;
	}
	D3D11_MAPPED_SUBRESOURCE mapped;
	if ( FAILED( CONTEXT->Map( *ppBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) )
		return false;
	memcpy( mapped.pData, pData, nBytes );
	CONTEXT->Unmap( *ppBuffer, 0 );
	return true;
}

//-----------------------------------------------------------------------------
// Static mesh vertex cache
//-----------------------------------------------------------------------------

static CachedVertices_t *FindCached( const void *pCacheKey )
{
	for ( size_t i = 0; i < s_VertexCache.size(); ++i )
		if ( s_VertexCache[i].m_pKey == pCacheKey )
			return &s_VertexCache[i];
	return NULL;
}

bool Dx11R_HasCachedVertices( const void *pCacheKey, int nVersion )
{
	CachedVertices_t *pCached = pCacheKey ? FindCached( pCacheKey ) : NULL;
	return pCached && pCached->m_nVersion == nVersion && pCached->m_pBuffer;
}

void Dx11R_ReleaseCachedVertices( const void *pCacheKey )
{
	for ( size_t i = 0; i < s_VertexCache.size(); ++i )
	{
		if ( s_VertexCache[i].m_pKey != pCacheKey )
			continue;
		if ( s_VertexCache[i].m_pBuffer )
			s_VertexCache[i].m_pBuffer->Release();
		s_VertexCache.erase( s_VertexCache.begin() + i );
		return;
	}
}

static ID3D11Buffer *CacheVertices( const void *pCacheKey, int nVersion, const Dx11Vertex_t *pVerts, int nVerts )
{
	CachedVertices_t *pCached = FindCached( pCacheKey );
	if ( pCached && pCached->m_nVersion == nVersion && pCached->m_pBuffer )
		return pCached->m_pBuffer;
	if ( !pVerts )
		return NULL;
	if ( !pCached )
	{
		CachedVertices_t entry = { pCacheKey, 0, 0, NULL };
		s_VertexCache.push_back( entry );
		pCached = &s_VertexCache.back();
	}
	if ( pCached->m_pBuffer )
		pCached->m_pBuffer->Release();
	pCached->m_pBuffer = NULL;
	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth = nVerts * sizeof( Dx11Vertex_t );
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA init = { pVerts, 0, 0 };
	if ( FAILED( DEVICE->CreateBuffer( &bd, &init, &pCached->m_pBuffer ) ) )
		return NULL;
	pCached->m_nVersion = nVersion;
	pCached->m_nVerts = nVerts;
	return pCached->m_pBuffer;
}

void Dx11R_Draw( Dx11Primitive_t type, const Dx11Vertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices,
	const void *pCacheKey, int nVersion )
{
	if ( !s_pVS || nVerts <= 0 || !CONTEXT )
		return;
	ID3D11Buffer *pVB = NULL;
	if ( pCacheKey )
	{
		pVB = CacheVertices( pCacheKey, nVersion, pVerts, nVerts );
	}
	else if ( pVerts && Upload( &s_pVB, &s_nVBSize, D3D11_BIND_VERTEX_BUFFER, pVerts, nVerts * sizeof( Dx11Vertex_t ) ) )
	{
		pVB = s_pVB;
	}
	if ( !pVB )
		return;
	ApplyState();
	++g_nDx11Draws;

	static const D3D11_PRIMITIVE_TOPOLOGY s_Topology[] =
	{
		D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, D3D11_PRIMITIVE_TOPOLOGY_LINELIST, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
	};
	CONTEXT->IASetPrimitiveTopology( s_Topology[type] );
	UINT nStride = sizeof( Dx11Vertex_t ), nOffset = 0;
	CONTEXT->IASetVertexBuffers( 0, 1, &pVB, &nStride, &nOffset );
	if ( pIndices && nIndices > 0 )
	{
		if ( !Upload( &s_pIB, &s_nIBSize, D3D11_BIND_INDEX_BUFFER, pIndices, nIndices * sizeof( unsigned short ) ) )
			return;
		CONTEXT->IASetIndexBuffer( s_pIB, DXGI_FORMAT_R16_UINT, 0 );
		CONTEXT->DrawIndexed( nIndices, 0, 0 );
	}
	else
	{
		CONTEXT->Draw( nVerts, 0 );
	}
}
