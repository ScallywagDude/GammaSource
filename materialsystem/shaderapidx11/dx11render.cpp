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
	ID3D11RenderTargetView *m_pRTV;
	ID3D11Texture2D *m_pDepth;
	ID3D11DepthStencilView *m_pDSV;
};
static int s_hRenderTarget;	// 0: the back buffer
static std::vector< Texture_t > s_Textures;	// handle = index + 1

static ID3D11VertexShader *s_pVS;
static ID3D11PixelShader *s_pPS;
static ID3D11InputLayout *s_pLayout;
static ID3D11Buffer *s_pConstants;
static ID3D11Buffer *s_pVB, *s_pIB;
static int s_nVBSize, s_nIBSize;
static ID3D11SamplerState *s_pSamplers[4];	// bit 0 clamp S, bit 1 clamp T
static ID3D11BlendState *s_pBlendStates[128];
static ID3D11DepthStencilState *s_pDepthStates[32];
static ID3D11RasterizerState *s_pRasterStates[2];
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
	float m_Pad;
};

static const char s_szShader[] =
	"cbuffer Constants : register( b0 ) { row_major float4x4 g_WVP; float g_AlphaRef; float g_AlphaTest; float g_Lightmap; float g_Pad; };\n"
	"Texture2D g_Texture : register( t0 ); SamplerState g_Sampler : register( s0 );\n"
	"Texture2D g_LightmapTex : register( t1 ); SamplerState g_LightmapSampler : register( s1 );\n"
	"struct VSIn { float3 pos : POSITION; float4 color : COLOR; float2 uv : TEXCOORD0; float2 uv2 : TEXCOORD1; };\n"
	"struct PSIn { float4 pos : SV_Position; float4 color : COLOR; float2 uv : TEXCOORD0; float2 uv2 : TEXCOORD1; };\n"
	"PSIn VSMain( VSIn v ) { PSIn o; o.pos = mul( float4( v.pos, 1.0f ), g_WVP ); o.color = v.color; o.uv = v.uv; o.uv2 = v.uv2; return o; }\n"
	"float4 PSMain( PSIn i ) : SV_Target { float4 c = g_Texture.Sample( g_Sampler, i.uv ) * i.color;\n"
	"  if ( g_Lightmap > 0.5f ) c.rgb *= g_LightmapTex.Sample( g_LightmapSampler, i.uv2 ).rgb * 2.0f;\n"
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
	};
	pDevice->CreateInputLayout( layout, 4, pVS->GetBufferPointer(), pVS->GetBufferSize(), &s_pLayout );
	pVS->Release(); pPS->Release();

	D3D11_BUFFER_DESC cb = {};
	cb.ByteWidth = sizeof( Constants_t );
	cb.Usage = D3D11_USAGE_DYNAMIC;
	cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	pDevice->CreateBuffer( &cb, NULL, &s_pConstants );

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
	for ( int i = 0; i < 2; ++i )
	{
		D3D11_RASTERIZER_DESC rd = {};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = i ? D3D11_CULL_BACK : D3D11_CULL_NONE;
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
	for ( int i = 0; i < 128; ++i ) SAFE_RELEASE( s_pBlendStates[i] );
	for ( int i = 0; i < 32; ++i ) SAFE_RELEASE( s_pDepthStates[i] );
	for ( int i = 0; i < 2; ++i ) SAFE_RELEASE( s_pRasterStates[i] );
	SAFE_RELEASE( s_pVB ); SAFE_RELEASE( s_pIB ); SAFE_RELEASE( s_pConstants );
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

int Dx11R_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget )
{
	Texture_t tex = {};
	tex.m_nWidth = nWidth > 0 ? nWidth : 1;
	tex.m_nHeight = nHeight > 0 ? nHeight : 1;
	tex.m_nMips = ( nMips > 0 && !bRenderTarget ) ? nMips : 1;
	tex.m_bRenderTarget = bRenderTarget;
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
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE | ( pTex->m_bRenderTarget ? D3D11_BIND_RENDER_TARGET : 0 );
	if ( FAILED( DEVICE->CreateTexture2D( &td, NULL, &pTex->m_pTexture ) ) )
		return false;
	if ( pTex->m_bRenderTarget && FAILED( DEVICE->CreateRenderTargetView( pTex->m_pTexture, NULL, &pTex->m_pRTV ) ) )
		return false;
	if ( FAILED( DEVICE->CreateShaderResourceView( pTex->m_pTexture, NULL, &pTex->m_pSRV ) ) )
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
	Mul( pModel, pView, mv );
	Mul( mv, pProj, s_WVP );
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
	int nBlendKey = s_State.m_bBlend ? ( 1 + nSrc * 11 + nDst ) % 128 : 0;
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
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
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
	pContext->RSSetState( s_pRasterStates[s_State.m_bCull ? 1 : 0] );
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
		Texture_t *pLightmap = GetTexture( s_hLightmap );
		ID3D11ShaderResourceView *pLightmapSRV = pLightmap ? pLightmap->m_pSRV : NULL;
		pContext->PSSetShaderResources( 1, 1, &pLightmapSRV );
		pContext->PSSetSamplers( 1, 1, &s_pSamplers[3] );
	}

	pContext->IASetInputLayout( s_pLayout );
	pContext->VSSetShader( s_pVS, NULL, 0 );
	pContext->VSSetConstantBuffers( 0, 1, &s_pConstants );
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

void Dx11R_Draw( Dx11Primitive_t type, const Dx11Vertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices )
{
	if ( !s_pVS || !pVerts || nVerts <= 0 || !CONTEXT )
		return;
	if ( !Upload( &s_pVB, &s_nVBSize, D3D11_BIND_VERTEX_BUFFER, pVerts, nVerts * sizeof( Dx11Vertex_t ) ) )
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
	CONTEXT->IASetVertexBuffers( 0, 1, &s_pVB, &nStride, &nOffset );
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
