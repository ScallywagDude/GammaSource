//========= GammaSource ============//
//
// GENERATED from materialsystem/shaderapidx11 by materialsystem/shaderapivk/make_vk.py - edit the DX11
// files and regenerate, so the two backends stay one implementation.
//
//=============================================================================//
//========= GammaSource ============//
//
// The API-independent half of the native renderers: the shader (HLSL, compiled at run time
// for DX11 and to SPIR-V for Vulkan), its constants layout, the per-pass state the shader
// API sets and the per-draw constants built from it. Included by dx11render.cpp; the Vulkan
// backend's copy is generated from this file by materialsystem/shaderapivk/make_vk.py.
//
//=============================================================================//

// Vulkan descriptor bindings (ignored by the D3D11 compiler)
#define GAMMA_SHADER_BINDINGS \
	"#ifdef __spirv__\n#define VKB( n ) [[vk::binding( n )]]\n#else\n#define VKB( n )\n#endif\n"

static int s_hBound;
static VkRState_t s_State;
static float s_WVP[16];
static float s_VP[16];
static float s_Bones[VKR_MAX_BONES][12];
static bool s_bBonesDirty = true;

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
	float m_Post[4];			// x: 1 bloom downsample, 2 blur x, 3 blur y; zw source texel size
	float m_PostParams[4];		// downsample: luminance weights + exponent; blur: x bloom amount
	float m_Fog[4];				// start, 1 / ( end - start ), max density, on
	float m_FogColor[4];
	float m_SelfIllum[4];		// rgb tint; w 1: base alpha masks it, 2: t3's red does
	float m_EnvParams[4];		// $envmapcontrast, $envmapsaturation
};

static const char s_szShader[] =
	GAMMA_SHADER_BINDINGS
	"VKB( 0 ) cbuffer Constants : register( b0 ) { row_major float4x4 g_WVP; float g_AlphaRef; float g_AlphaTest; float g_Lightmap; float g_Lit;\n"
	"  row_major float4x4 g_Model; float4 g_Ambient[6]; float4 g_LightColor[4]; float4 g_LightPos[4]; float4 g_LightDir[4];\n"
	"  float4 g_LightAtten[4]; float4 g_LightCone[4]; float4 g_ShadowColor; float g_Shadow; float g_Skin; float2 g_Pad2;\n"
	"  float4 g_VColorUse; float4 g_MatColor;\n"
	"  row_major float4x4 g_VP; float4 g_Water; float4 g_RefractTint; float4 g_ReflectTint; float4 g_Clip[2]; float4 g_EnvTint; float4 g_EyePos;\n"
	"  float4 g_Post; float4 g_PostParams; float4 g_Fog; float4 g_FogColor; float4 g_SelfIllum; float4 g_EnvParams; };\n"
	"VKB( 6 ) TextureCube g_Envmap : register( t2 ); VKB( 7 ) SamplerState g_EnvSampler : register( s2 );\n"
	"VKB( 1 ) cbuffer Bones : register( b1 ) { float4 g_Bones[53 * 3]; };\n"
	"VKB( 2 ) Texture2D g_Texture : register( t0 ); VKB( 3 ) SamplerState g_Sampler : register( s0 );\n"
	"VKB( 8 ) Texture2D g_MaskTex : register( t3 );\n"
	"VKB( 4 ) Texture2D g_LightmapTex : register( t1 ); VKB( 5 ) SamplerState g_LightmapSampler : register( s1 );\n"
	"struct VSIn { float3 pos : POSITION; float4 color : COLOR; float2 uv : TEXCOORD0; float2 uv2 : TEXCOORD1; float3 normal : NORMAL;\n"
	"  uint4 bones : BLENDINDICES; float2 weights : BLENDWEIGHT; };\n"
	"float3 SkinPos( int b, float3 p ) { return float3( dot( g_Bones[b * 3], float4( p, 1 ) ), dot( g_Bones[b * 3 + 1], float4( p, 1 ) ), dot( g_Bones[b * 3 + 2], float4( p, 1 ) ) ); }\n"
	"float3 SkinDir( int b, float3 d ) { return float3( dot( g_Bones[b * 3].xyz, d ), dot( g_Bones[b * 3 + 1].xyz, d ), dot( g_Bones[b * 3 + 2].xyz, d ) ); }\n"
	"struct PSIn { float4 pos : SV_Position; float4 color : COLOR; float2 uv : TEXCOORD0; float2 uv2 : TEXCOORD1; float2 clip : SV_ClipDistance0;\n"
	"  float3 wpos : TEXCOORD2; float3 wnorm : TEXCOORD3; float fogz : TEXCOORD4; };\n"
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
	"  o.wpos = p; o.wnorm = n; o.fogz = o.pos.w;\n"
	"  o.clip = float2( dot( g_Clip[0].xyz, p ) - g_Clip[0].w, dot( g_Clip[1].xyz, p ) - g_Clip[1].w );\n"
	"  if ( g_Lit > 0.5f ) { n = normalize( n + 1e-6f );\n"
	"    float3 L = Lighting( p, n ); if ( g_Lit > 1.5f ) L += pow( v.color.rgb * 2.0f, float3( 2.2f, 2.2f, 2.2f ) );\n"
	"    o.color.rgb *= pow( saturate( L ), float3( 1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f ) ); }\n"
	"  return o; }\n"
	"float4 PSMain( PSIn i ) : SV_Target {\n"
	"  if ( g_Post.x > 0.5f ) { float2 t = g_Post.zw; float4 r = float4( 0.0f, 0.0f, 0.0f, 0.0f );\n"
	"    if ( g_Post.x < 1.5f ) { for ( int k = 0; k < 4; ++k ) {\n"
	"      float4 p = g_Texture.Sample( g_Sampler, i.uv + float2( ( k == 1 || k == 3 ) ? 2.5f : 0.5f, k >= 2 ? 2.5f : 0.5f ) * t );\n"
	"      p.rgb = pow( p.rgb, g_PostParams.www ) * dot( p.rgb, g_PostParams.rgb ); r += p * 0.25f; } return r; }\n"
	"    float2 d = g_Post.x < 2.5f ? float2( t.x, 0.0f ) : float2( 0.0f, t.y );\n"
	"    r = g_Texture.Sample( g_Sampler, i.uv ) * 0.2013f;\n"
	"    r += ( g_Texture.Sample( g_Sampler, i.uv + d * 1.3366f ) + g_Texture.Sample( g_Sampler, i.uv - d * 1.3366f ) ) * 0.2185f;\n"
	"    r += ( g_Texture.Sample( g_Sampler, i.uv + d * 3.4295f ) + g_Texture.Sample( g_Sampler, i.uv - d * 3.4295f ) ) * 0.0821f;\n"
	"    r += ( g_Texture.Sample( g_Sampler, i.uv + d * 5.4264f ) + g_Texture.Sample( g_Sampler, i.uv - d * 5.4264f ) ) * 0.0461f;\n"
	"    r += ( g_Texture.Sample( g_Sampler, i.uv + d * 7.4359f ) + g_Texture.Sample( g_Sampler, i.uv - d * 7.4359f ) ) * 0.0262f;\n"
	"    r += ( g_Texture.Sample( g_Sampler, i.uv + d * 9.4436f ) + g_Texture.Sample( g_Sampler, i.uv - d * 9.4436f ) ) * 0.0162f;\n"
	"    r += ( g_Texture.Sample( g_Sampler, i.uv + d * 11.4401f ) + g_Texture.Sample( g_Sampler, i.uv - d * 11.4401f ) ) * 0.0102f;\n"
	"    r.rgb *= g_PostParams.x; return r; }\n"
	"  float4 c = g_Texture.Sample( g_Sampler, i.uv ) * i.color;\n"
	"  if ( g_Lightmap > 0.5f ) c.rgb *= g_LightmapTex.Sample( g_LightmapSampler, i.uv2 ).rgb * 2.0f;\n"
	"  if ( g_SelfIllum.w > 0.5f ) { float4 sb = g_Texture.Sample( g_Sampler, i.uv );\n"
	"    float sm = g_SelfIllum.w > 1.5f ? g_MaskTex.Sample( g_Sampler, i.uv ).r : sb.a;\n"
	"    c.rgb = lerp( c.rgb, sb.rgb * g_SelfIllum.rgb, sm ); c.a = i.color.a; }\n"
	"  if ( g_EyePos.w > 0.5f ) { float3 v = normalize( i.wpos - g_EyePos.xyz ); float3 rv = reflect( v, normalize( i.wnorm + 1e-6f ) );\n"
	"    float4 m = g_MaskTex.Sample( g_Sampler, i.uv ); float4 b = g_Texture.Sample( g_Sampler, i.uv );\n"
	"    float3 mask = g_EnvTint.w > 2.5f ? m.aaa : ( g_EnvTint.w > 1.5f ? m.rgb : ( g_EnvTint.w > 0.5f ? 1.0f - b.aaa : float3( 1.0f, 1.0f, 1.0f ) ) );\n"
	"    float3 spec = g_Envmap.Sample( g_EnvSampler, rv ).rgb * g_EnvTint.rgb * mask; spec = lerp( spec, spec * spec, g_EnvParams.x );\n"
	"    float grey = dot( spec, float3( 0.299f, 0.587f, 0.114f ) ); c.rgb += lerp( float3( grey, grey, grey ), spec, g_EnvParams.y ); }\n"
	"  if ( g_Water.x > 0.5f ) { float2 s = i.pos.xy * g_Water.yz;\n"
	"    float3 refr = g_Texture.Sample( g_Sampler, s ).rgb * g_RefractTint.rgb;\n"
	"    float3 refl = g_LightmapTex.Sample( g_LightmapSampler, float2( s.x, 1.0f - s.y ) ).rgb * g_ReflectTint.rgb;\n"
	"    float3 eye = normalize( g_EyePos.xyz - i.wpos ); float fresnel = pow( 1.0f - saturate( abs( eye.z ) ), 5.0f );\n"
	"    return float4( lerp( lerp( refr, refl, saturate( 0.2f + 0.8f * fresnel ) ), g_FogColor.rgb, ( g_Fog.w > 0.5f ? saturate( ( i.fogz - g_Fog.x ) * g_Fog.y ) * g_Fog.z : 0.0f ) ), 1.0f ); }\n"
	"  if ( g_Shadow > 0.5f ) { float cover = saturate( g_Texture.Sample( g_Sampler, i.uv ).a - i.color.a );\n"
	"    return float4( 1.0f + cover * ( g_ShadowColor.rgb - 1.0f ), 1.0f ); }\n"
	"  if ( g_AlphaTest > 0.5f ) clip( c.a - g_AlphaRef );\n"
	"  c.rgb = lerp( c.rgb, g_FogColor.rgb, ( g_Fog.w > 0.5f ? saturate( ( i.fogz - g_Fog.x ) * g_Fog.y ) * g_Fog.z : 0.0f ) ); return c; }\n";


static int s_hLightmap;
static int s_hStages[16];		// what each sampler stage has bound (0 is s_hBound)
#define s_hStage1 s_hStages[1]
#define s_hStage2 s_hStages[2]
static bool s_bEnvmap;
static int s_hEnvMask;			// t3: the envmap mask texture
static float s_EnvTint[4], s_EyePos[4];
static float s_EnvParams[4] = { 0.0f, 1.0f, 0.0f, 0.0f };

// after SetEnvmap: $envmapcontrast (0 as is, 1 squared) and $envmapsaturation (0 grey, 1 as is)
void VkR_SetEnvmapParams( float flContrast, float flSaturation )
{
	s_EnvParams[0] = flContrast;
	s_EnvParams[1] = flSaturation;
}

void VkR_SetEnvmap( bool bEnable, const float *pTint, int nMask, const float *pEyePos, int hMask )
{
	s_bEnvmap = bEnable;
	s_hEnvMask = bEnable ? hMask : 0;
	if ( !bEnable )
		return;
	for ( int i = 0; i < 3; ++i )
	{
		s_EnvTint[i] = pTint ? pTint[i] : 1.0f;
		s_EyePos[i] = pEyePos ? pEyePos[i] : 0.0f;
	}
	s_EnvTint[3] = (float)nMask;
	s_EnvParams[0] = 0.0f;
	s_EnvParams[1] = 1.0f;
	s_EyePos[3] = 1.0f;
}
static bool s_bLit;
static bool s_bStaticLight;		// the vertex colour is vrad's light (a static prop's colour mesh)

void VkR_SetStaticLight( bool bStatic )
{
	s_bStaticLight = bStatic;
}
static bool s_bShadowPass;


static bool s_bWaterPass;
static float s_ClipPlanes[2][4] = { { 0, 0, 0, -1 }, { 0, 0, 0, -1 } };

void VkR_SetClipPlane( int nIndex, const float *pPlane, bool bEnable )
{
	if ( nIndex < 0 || nIndex >= 2 )
		return;
	static const float s_Off[4] = { 0, 0, 0, -1 };
	memcpy( s_ClipPlanes[nIndex], ( bEnable && pPlane ) ? pPlane : s_Off, sizeof( s_Off ) );
}
static int s_hWaterReflection;
static float s_RefractTint[4] = { 1, 1, 1, 1 }, s_ReflectTint[4] = { 1, 1, 1, 1 };

void VkR_SetWaterPass( bool bWater, int hReflection, const float *pRefractTint, const float *pReflectTint )
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

void VkR_SetVertexColorUse( bool bColor, bool bAlpha, const float *pColor )
{
	s_VColorUse[0] = bColor ? 1.0f : 0.0f;
	s_VColorUse[1] = bAlpha ? 1.0f : 0.0f;
	for ( int i = 0; i < 4; ++i )
		s_MatColor[i] = pColor ? pColor[i] : 1.0f;
}

void VkR_SetBoneMatrices( const float ( *pBones )[12], int nCount )
{
	if ( nCount > VKR_MAX_BONES ) nCount = VKR_MAX_BONES;
	if ( nCount > 0 && memcmp( s_Bones, pBones, nCount * sizeof( s_Bones[0] ) ) )
	{
		memcpy( s_Bones, pBones, nCount * sizeof( s_Bones[0] ) );
		s_bBonesDirty = true;
	}
}
static float s_ShadowColor[4];
static float s_Post[4], s_PostParams[4];
static float s_SelfIllum[4];

// $selfillum: nMask 1 base alpha, 2 hMask (bound on t3, so only without an envmap mask)
void VkR_SetSelfIllum( int nMask, const float *pTint, int hMask )
{
	for ( int i = 0; i < 3; ++i )
		s_SelfIllum[i] = pTint ? pTint[i] : 1.0f;
	s_SelfIllum[3] = (float)nMask;
	if ( nMask == 2 )
		s_hEnvMask = hMask;
}
static float s_SceneFog[4], s_SceneFogColor[3];	// see VkR_SetSceneFog

void VkR_SetSceneFog( bool bEnable, float flStart, float flEnd, float flMaxDensity, const unsigned char *pColor )
{
	s_SceneFog[0] = flStart;
	s_SceneFog[1] = flEnd > flStart ? 1.0f / ( flEnd - flStart ) : 0.0f;
	s_SceneFog[2] = flMaxDensity;
	s_SceneFog[3] = ( bEnable && flEnd > flStart ) ? 1.0f : 0.0f;
	for ( int i = 0; i < 3; ++i )
		s_SceneFogColor[i] = pColor ? pColor[i] / 255.0f : 0.0f;
}

// Source's LDR bloom passes (see SetPostPass in dx11render.h)
void VkR_SetPostPass( int nMode, const float *pParams, int nTexWidth, int nTexHeight )
{
	s_Post[0] = (float)nMode;
	s_Post[2] = nTexWidth > 0 ? 1.0f / nTexWidth : 0.0f;
	s_Post[3] = nTexHeight > 0 ? 1.0f / nTexHeight : 0.0f;
	for ( int i = 0; i < 4; ++i )
		s_PostParams[i] = pParams ? pParams[i] : 1.0f;
}

void VkR_SetShadowPass( bool bShadow, const float *pShadowColor )
{
	s_bShadowPass = bShadow;
	if ( bShadow && pShadowColor )
		memcpy( s_ShadowColor, pShadowColor, sizeof( s_ShadowColor ) );
}
static float s_Model[16];
static float s_ViewEye[3];
static float s_Ambient[6][3];
static int s_nLights;
static VkRLight_t s_Lights[4];

void VkR_SetLighting( bool bLit, const float ambientCube[6][3], int nLights, const VkRLight_t *pLights )
{
	static int s_nNoLit = -1;
	if ( s_nNoLit < 0 ) { char b[8]; s_nNoLit = GetEnvironmentVariableA( "GAMMA_VK_NOLIT", b, 8 ) ? 1 : 0; }
	s_bLit = bLit && !s_nNoLit;
	if ( !s_bLit )
		return;
	memcpy( s_Ambient, ambientCube, sizeof( s_Ambient ) );
	s_nLights = nLights < 4 ? nLights : 4;
	for ( int i = 0; i < s_nLights; ++i )
		s_Lights[i] = pLights[i];
}

void VkR_BindLightmap( int hTexture )
{
	s_hLightmap = hTexture;
}

void VkR_BindTexture( int nStage, int hTexture )
{
	extern int g_nVkBinds;
	if ( hTexture > 0 ) ++g_nVkBinds;
	if ( nStage == 0 )
		s_hBound = hTexture;
	if ( nStage >= 0 && nStage < 16 )
		s_hStages[nStage] = hTexture;
}

int VkR_GetBoundTexture( int nStage )
{
	return nStage == 0 ? s_hBound : ( nStage > 0 && nStage < 16 ) ? s_hStages[nStage] : 0;
}

//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------
void VkR_SetState( const VkRState_t &state )
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

void VkR_SetMatrices( const float *pModel, const float *pView, const float *pProj )
{
	float mv[16];
	memcpy( s_Model, pModel, sizeof( s_Model ) );
	Mul( pModel, pView, mv );
	Mul( mv, pProj, s_WVP );
	Mul( pView, pProj, s_VP );
	// the eye: the inverse of the view matrix's translation (row vectors)
	for ( int i = 0; i < 3; ++i )
		s_ViewEye[i] = -( pView[12] * pView[i * 4 + 0] + pView[13] * pView[i * 4 + 1] + pView[14] * pView[i * 4 + 2] );
}


//-----------------------------------------------------------------------------
// The per-draw shader constants from the pass state above
//-----------------------------------------------------------------------------
static void FillConstants( Constants_t *pConst, bool bHaveLightmap, int nTargetW, int nTargetH )
{
	pConst->m_Lightmap = bHaveLightmap ? 1.0f : 0.0f;
	memcpy( pConst->m_WVP, s_WVP, sizeof( s_WVP ) );
	pConst->m_AlphaRef = s_State.m_flAlphaRef;
	pConst->m_AlphaTest = s_State.m_bAlphaTest ? 1.0f : 0.0f;
	pConst->m_Shadow = s_bShadowPass ? 1.0f : 0.0f;
	pConst->m_Skin = (float)s_State.m_nBoneWeights;
	memcpy( pConst->m_VColorUse, s_VColorUse, sizeof( s_VColorUse ) );
	pConst->m_Water[0] = s_bWaterPass ? 1.0f : 0.0f;
	{
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
	if ( s_bWaterPass )	// water fresnel: the reflection grows toward grazing angles
		memcpy( pConst->m_EyePos, s_ViewEye, sizeof( s_ViewEye ) );
	memcpy( pConst->m_MatColor, s_MatColor, sizeof( s_MatColor ) );
	memcpy( pConst->m_Post, s_Post, sizeof( s_Post ) );
	memcpy( pConst->m_SelfIllum, s_SelfIllum, sizeof( s_SelfIllum ) );
	memcpy( pConst->m_EnvParams, s_EnvParams, sizeof( s_EnvParams ) );
	// fog: the pass's mode picks the colour it fades to (additive passes fade to black)
	memcpy( pConst->m_Fog, s_SceneFog, sizeof( s_SceneFog ) );
	pConst->m_FogColor[3] = 1.0f;
	switch ( s_State.m_nFogMode )
	{
	case 0: pConst->m_Fog[3] = 0.0f; break;					// SHADER_FOGMODE_DISABLED
	case 1: for ( int i = 0; i < 3; ++i ) pConst->m_FogColor[i] = s_SceneFogColor[i] * 0.5f; break;	// OO_OVERBRIGHT
	case 2: break;												// BLACK
	case 3: pConst->m_FogColor[0] = pConst->m_FogColor[1] = pConst->m_FogColor[2] = 0.5f; break;		// GREY
	case 5: pConst->m_FogColor[0] = pConst->m_FogColor[1] = pConst->m_FogColor[2] = 1.0f; break;		// WHITE
	default: memcpy( pConst->m_FogColor, s_SceneFogColor, sizeof( s_SceneFogColor ) ); break;	// FOGCOLOR
	}
	if ( s_Post[0] > 0.5f || s_bShadowPass )
		pConst->m_Fog[3] = 0.0f;
	memcpy( pConst->m_PostParams, s_PostParams, sizeof( s_PostParams ) );
	memcpy( pConst->m_VP, s_VP, sizeof( s_VP ) );
	for ( int i = 0; i < 3; ++i )	// the constant is linear; this shader works in gamma space
		pConst->m_ShadowColor[i] = powf( s_ShadowColor[i] > 0.0f ? s_ShadowColor[i] : 0.0f, 1.0f / 2.2f );
	pConst->m_ShadowColor[3] = 1.0f;
	// baked static prop light needs no normals; dynamic lights and the ambient cube do
	bool bDynamicLit = s_bLit && s_State.m_bNormals;
	bool bLit = bDynamicLit || s_bStaticLight;
	pConst->m_Lit = bLit ? ( s_bStaticLight ? 2.0f : 1.0f ) : 0.0f;
	memcpy( pConst->m_Model, s_Model, sizeof( s_Model ) );
	memset( pConst->m_Ambient, 0, sizeof( pConst->m_Ambient ) + sizeof( pConst->m_LightColor ) + sizeof( pConst->m_LightPos ) +
		sizeof( pConst->m_LightDir ) + sizeof( pConst->m_LightAtten ) + sizeof( pConst->m_LightCone ) );
	if ( bDynamicLit )
	{
		for ( int i = 0; i < 6; ++i )
			memcpy( pConst->m_Ambient[i], s_Ambient[i], sizeof( s_Ambient[i] ) );
		for ( int i = 0; i < s_nLights; ++i )
		{
			const VkRLight_t &l = s_Lights[i];
			memcpy( pConst->m_LightColor[i], l.m_Color, 12 ); pConst->m_LightColor[i][3] = (float)l.m_nType;
			memcpy( pConst->m_LightPos[i], l.m_Pos, 12 ); pConst->m_LightPos[i][3] = l.m_flRange;
			memcpy( pConst->m_LightDir[i], l.m_Dir, 12 );
			memcpy( pConst->m_LightAtten[i], l.m_flAtten, 12 ); pConst->m_LightAtten[i][3] = l.m_flFalloff;
			pConst->m_LightCone[i][0] = l.m_flThetaDot; pConst->m_LightCone[i][1] = l.m_flPhiDot;
		}
	}
}
