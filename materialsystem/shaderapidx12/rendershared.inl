//========= GammaSource ============//
//
// GENERATED from materialsystem/shaderapidx11 by materialsystem/shaderapidx12/make_dx12.py - edit the
// DX11 files and regenerate, so the backends stay one implementation.
//
//=============================================================================//
//========= GammaSource ============//
//
// The API-independent half of the native renderers: the shader (HLSL, compiled at run time
// for DX12 and to SPIR-V for Vulkan), its constants layout, the per-pass state the shader
// API sets and the per-draw constants built from it. Included by dx12render.cpp; the Vulkan
// backend's copy is generated from this file by materialsystem/shaderapivk/make_vk.py.
//
//=============================================================================//

// Vulkan descriptor bindings (ignored by the D3D compiler)
#define GAMMA_SHADER_BINDINGS \
	"#ifdef __spirv__\n#define VKB( n ) [[vk::binding( n )]]\n#else\n#define VKB( n )\n#endif\n"

static int s_hBound;
static Dx12State_t s_State;
static float s_WVP[16];
static float s_VP[16];
static float s_Bones[DX12_MAX_BONES][12];
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
	float m_EnvParams[4];		// $envmapcontrast, $envmapsaturation, $envmapfresnel, skin shader
	float m_Char[4];			// x: 1 Eyes, 2 EyeRefract, 3 Teeth; y: eye glint damping
	float m_CharConst[6][4];	// the shader's vertex constants (see Dx12R_SetCharacterPass)
	float m_Bump[4];			// x: 1 world (bumped lightmaps), 2 model (per pixel lights), 3 water; y: ssbump;
								// zw: water refract / reflect amounts
	float m_Detail[4];			// x: $detailblendmode + 1 (0 off); yz: $detailscale; w: $detailblendfactor
	float m_Base2[4];			// x: 1 blend t6 ($basetexture2) by vertex alpha
	float m_BaseTx[2][4];		// the base (or water bump) texture transform's u and v rows
	float m_Phong[4];			// $phongexponent (0 off), $phongboost, mask: 0 normal alpha, 1 base alpha
	float m_PhongFresnel[4];	// $phongfresnelranges
	float m_PhongTint[4];
	float m_ViewEye[4];
};

static const char s_szShader[] =
	GAMMA_SHADER_BINDINGS
	"VKB( 0 ) cbuffer Constants : register( b0 ) { row_major float4x4 g_WVP; float g_AlphaRef; float g_AlphaTest; float g_Lightmap; float g_Lit;\n"
	"  row_major float4x4 g_Model; float4 g_Ambient[6]; float4 g_LightColor[4]; float4 g_LightPos[4]; float4 g_LightDir[4];\n"
	"  float4 g_LightAtten[4]; float4 g_LightCone[4]; float4 g_ShadowColor; float g_Shadow; float g_Skin; float2 g_Pad2;\n"
	"  float4 g_VColorUse; float4 g_MatColor;\n"
	"  row_major float4x4 g_VP; float4 g_Water; float4 g_RefractTint; float4 g_ReflectTint; float4 g_Clip[2]; float4 g_EnvTint; float4 g_EyePos;\n"
	"  float4 g_Post; float4 g_PostParams; float4 g_Fog; float4 g_FogColor; float4 g_SelfIllum; float4 g_EnvParams;\n"
	"  float4 g_Char; float4 g_CharC[6];\n"
	"  float4 g_Bump; float4 g_Detail; float4 g_Base2; float4 g_BaseTx[2]; float4 g_Phong; float4 g_PhongFresnel; float4 g_PhongTint; float4 g_ViewEye; };\n"
	"VKB( 6 ) TextureCube g_Envmap : register( t2 ); VKB( 7 ) SamplerState g_EnvSampler : register( s2 );\n"
	"VKB( 1 ) cbuffer Bones : register( b1 ) { float4 g_Bones[53 * 3]; };\n"
	"VKB( 2 ) Texture2D g_Texture : register( t0 ); VKB( 3 ) SamplerState g_Sampler : register( s0 );\n"
	"VKB( 8 ) Texture2D g_MaskTex : register( t3 );\n"
	"VKB( 9 ) Texture2D g_BumpTex : register( t4 ); VKB( 10 ) Texture2D g_DetailTex : register( t5 ); VKB( 11 ) Texture2D g_Base2Tex : register( t6 );\n"
	"VKB( 12 ) SamplerState g_WrapSampler : register( s3 );\n"
	"VKB( 4 ) Texture2D g_LightmapTex : register( t1 ); VKB( 5 ) SamplerState g_LightmapSampler : register( s1 );\n"
	"struct VSIn { float3 pos : POSITION; float4 color : COLOR; float2 uv : TEXCOORD0; float2 uv2 : TEXCOORD1; float3 normal : NORMAL;\n"
	"  uint4 bones : BLENDINDICES; float2 weights : BLENDWEIGHT; float3 tangentS : TANGENT; float3 tangentT : BINORMAL; float lmofs : TEXCOORD2; };\n"
	"float3 SkinPos( int b, float3 p ) { return float3( dot( g_Bones[b * 3], float4( p, 1 ) ), dot( g_Bones[b * 3 + 1], float4( p, 1 ) ), dot( g_Bones[b * 3 + 2], float4( p, 1 ) ) ); }\n"
	"float3 SkinDir( int b, float3 d ) { return float3( dot( g_Bones[b * 3].xyz, d ), dot( g_Bones[b * 3 + 1].xyz, d ), dot( g_Bones[b * 3 + 2].xyz, d ) ); }\n"
	"struct PSIn { float4 pos : SV_Position; float4 color : COLOR; float2 uv : TEXCOORD0; float2 uv2 : TEXCOORD1; float2 clip : SV_ClipDistance0;\n"
	"  float3 wpos : TEXCOORD2; float3 wnorm : TEXCOORD3; float fogz : TEXCOORD4; float4 eye : TEXCOORD5;\n"
	"  float3 tan : TEXCOORD6; float4 bin : TEXCOORD7; float4 uv3 : TEXCOORD8; };\n"
	"float3 AmbientCube( float3 n ) { float3 s = n * n; int3 i = n < 0.0f ? int3( 1, 3, 5 ) : int3( 0, 2, 4 );\n"
	"  return s.x * g_Ambient[i.x].rgb + s.y * g_Ambient[i.y].rgb + s.z * g_Ambient[i.z].rgb; }\n"
	"float3 LightingSpec( float3 p, float3 n, float3 r, float e, out float3 spec ) { float3 l = AmbientCube( n ); spec = float3( 0.0f, 0.0f, 0.0f );\n"
	"  [unroll] for ( int k = 0; k < 4; ++k ) { float type = g_LightColor[k].w; if ( type < 0.5f ) continue;\n"
	"    float3 dir = -g_LightDir[k].xyz; float att = 1.0f;\n"
	"    if ( type < 1.5f || type > 2.5f ) { float3 d = g_LightPos[k].xyz - p; float dist = length( d ); dir = d / max( dist, 0.001f );\n"
	"      att = 1.0f / max( g_LightAtten[k].x + g_LightAtten[k].y * dist + g_LightAtten[k].z * dist * dist, 0.0001f );\n"
	"      if ( g_LightPos[k].w > 0.0f && dist > g_LightPos[k].w ) att = 0.0f;\n"
	"      if ( type > 2.5f ) { float c = dot( -dir, g_LightDir[k].xyz );\n"
	"        float cone = saturate( ( c - g_LightCone[k].y ) / max( g_LightCone[k].x - g_LightCone[k].y, 0.0001f ) );\n"
	"        att *= pow( cone, max( g_LightAtten[k].w, 0.0001f ) ); } }\n"
	"    float nl = saturate( dot( n, dir ) ); l += g_LightColor[k].rgb * att * nl;\n"
	"    if ( e > 0.0f ) spec += g_LightColor[k].rgb * att * nl * pow( saturate( dot( r, dir ) ), e ); }\n"
	"  return l; }\n"
	"float3 Lighting( float3 p, float3 n ) { float3 s; return LightingSpec( p, n, float3( 0.0f, 0.0f, 1.0f ), 0.0f, s ); }\n"
	"float4 Detail( float4 b, float2 uv ) { float4 d = g_DetailTex.Sample( g_WrapSampler, uv ); float f = g_Detail.w; float m = g_Detail.x - 1.0f;\n"
	"  if ( m < 0.5f || ( m > 6.5f && m < 7.5f ) ) b.rgb *= lerp( float3( 1.0f, 1.0f, 1.0f ), 2.0f * d.rgb, f );\n"
	"  else if ( m < 1.5f ) b.rgb += f * d.rgb;\n"
	"  else if ( m < 2.5f ) b.rgb = lerp( b.rgb, d.rgb, f * d.a );\n"
	"  else if ( m < 3.5f ) b.rgb = lerp( b.rgb, d.rgb, f );\n"
	"  else if ( m < 4.5f ) b.rgb = lerp( d.rgb, b.rgb, b.a );\n"
	"  else if ( m > 7.5f && m < 8.5f ) b *= lerp( float4( 1.0f, 1.0f, 1.0f, 1.0f ), d, f );\n"
	"  else if ( m > 8.5f && m < 9.5f ) b.a = lerp( b.a, b.a * d.a, f );\n"
	"  return b; }\n"
	"PSIn VSMain( VSIn v ) { PSIn o; o.color = lerp( 1.0f, v.color, g_VColorUse.xxxy ) * g_MatColor; o.uv2 = v.uv2;\n"
	"  o.uv = float2( dot( g_BaseTx[0], float4( v.uv, 0.0f, 1.0f ) ), dot( g_BaseTx[1], float4( v.uv, 0.0f, 1.0f ) ) );\n"
	"  o.uv3 = float4( v.uv * g_Detail.yz, v.color.a, 0.0f );\n"
	"  float3 p, n, t, b;\n"
	"  if ( g_Skin > 0.5f ) { float3 w = float3( v.weights.x, g_Skin > 1.5f ? v.weights.y : 0.0f, 0.0f ); w.z = g_Skin > 2.5f ? 1.0f - w.x - w.y : 0.0f;\n"
	"    if ( g_Skin < 1.5f ) w.x = 1.0f; else if ( g_Skin < 2.5f ) w.y = 1.0f - w.x;\n"
	"    p = SkinPos( v.bones.x, v.pos ) * w.x + SkinPos( v.bones.y, v.pos ) * w.y + SkinPos( v.bones.z, v.pos ) * w.z;\n"
	"    n = SkinDir( v.bones.x, v.normal ) * w.x + SkinDir( v.bones.y, v.normal ) * w.y + SkinDir( v.bones.z, v.normal ) * w.z;\n"
	"    t = SkinDir( v.bones.x, v.tangentS ) * w.x + SkinDir( v.bones.y, v.tangentS ) * w.y + SkinDir( v.bones.z, v.tangentS ) * w.z;\n"
	"    b = SkinDir( v.bones.x, v.tangentT ) * w.x + SkinDir( v.bones.y, v.tangentT ) * w.y + SkinDir( v.bones.z, v.tangentT ) * w.z;\n"
	"    o.pos = mul( float4( p, 1.0f ), g_VP ); }\n"
	"  else { o.pos = mul( float4( v.pos, 1.0f ), g_WVP ); p = mul( float4( v.pos, 1.0f ), g_Model ).xyz; n = mul( v.normal, (float3x3)g_Model );\n"
	"    t = mul( v.tangentS, (float3x3)g_Model ); b = mul( v.tangentT, (float3x3)g_Model ); }\n"
	"  if ( g_Char.x > 0.5f && g_Char.x < 2.5f ) { float3 e = p - g_CharC[0].xyz; n = e - 0.5f * dot( e, g_CharC[1].xyz ) * g_CharC[1].xyz; }\n"
	"  o.eye = float4( dot( g_CharC[2], float4( p, 1.0f ) ), dot( g_CharC[3], float4( p, 1.0f ) ), dot( g_CharC[4], float4( p, 1.0f ) ), dot( g_CharC[5], float4( p, 1.0f ) ) );\n"
	"  o.wpos = p; o.wnorm = n; o.fogz = o.pos.w; o.tan = t; o.bin = float4( b, v.lmofs );\n"
	"  o.clip = float2( dot( g_Clip[0].xyz, p ) - g_Clip[0].w, dot( g_Clip[1].xyz, p ) - g_Clip[1].w );\n"
	"  if ( g_Lit > 0.5f && !( g_Bump.x > 1.5f && g_Bump.x < 2.5f && g_Lit < 1.5f ) ) { n = normalize( n + 1e-6f );\n"
	"    float3 L = Lighting( p, n ); if ( g_Lit > 1.5f ) L += pow( v.color.rgb * 2.0f, float3( 2.2f, 2.2f, 2.2f ) );\n"
	"    if ( g_Char.x > 2.5f ) L *= g_CharC[0].w * saturate( dot( n, g_CharC[0].xyz ) );\n"
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
	"  float4 base = g_Texture.Sample( g_Sampler, i.uv );\n"
	"  if ( g_Base2.x > 0.5f ) base = lerp( base, g_Base2Tex.Sample( g_WrapSampler, i.uv ), saturate( i.uv3.z ) );\n"
	"  if ( g_Detail.x > 0.5f ) base = Detail( base, i.uv3.xy );\n"
	"  float4 c = base * i.color; float3 bn = normalize( i.wnorm + 1e-6f ); float4 nm = float4( 0.5f, 0.5f, 1.0f, 1.0f );\n"
	"  if ( g_Bump.x > 0.5f && g_Bump.x < 2.5f ) { nm = g_BumpTex.Sample( g_WrapSampler, i.uv );\n"
	"    if ( g_Bump.y < 0.5f ) { float3 tn = nm.xyz * 2.0f - 1.0f; bn = normalize( tn.x * i.tan + tn.y * i.bin.xyz + tn.z * i.wnorm + 1e-6f ); } }\n"
	"  float pm = g_Phong.z > 0.5f ? base.a : nm.a;\n"
	"  if ( g_Lightmap > 0.5f && g_Char.x < 0.5f ) { float3 lm = g_LightmapTex.Sample( g_LightmapSampler, i.uv2 ).rgb;\n"
	"    if ( g_Bump.x > 0.5f && g_Bump.x < 1.5f && i.bin.w > 0.0f ) { float3 w = nm.xyz;\n"
	"      if ( g_Bump.y < 0.5f ) { float3 tn = normalize( nm.xyz * 2.0f - 1.0f );\n"
	"        w = saturate( float3( dot( tn, float3( 0.81649658f, 0.0f, 0.57735027f ) ), dot( tn, float3( -0.40824830f, 0.70710678f, 0.57735027f ) ),\n"
	"          dot( tn, float3( -0.40824830f, -0.70710678f, 0.57735027f ) ) ) ); w *= w; w /= max( dot( w, float3( 1.0f, 1.0f, 1.0f ) ), 0.0001f ); }\n"
	"      lm = w.x * g_LightmapTex.Sample( g_LightmapSampler, i.uv2 + float2( i.bin.w, 0.0f ) ).rgb +\n"
	"        w.y * g_LightmapTex.Sample( g_LightmapSampler, i.uv2 + float2( 2.0f * i.bin.w, 0.0f ) ).rgb +\n"
	"        w.z * g_LightmapTex.Sample( g_LightmapSampler, i.uv2 + float2( 3.0f * i.bin.w, 0.0f ) ).rgb; }\n"
	"    c.rgb *= lm * 2.0f; }\n"
	"  if ( g_Bump.x > 1.5f && g_Bump.x < 2.5f && g_Lit > 0.5f && g_Lit < 1.5f ) { float3 ev = normalize( g_ViewEye.xyz - i.wpos ); float3 spec;\n"
	"    float3 L = LightingSpec( i.wpos, bn, reflect( -ev, bn ), g_Phong.x, spec );\n"
	"    c.rgb *= pow( saturate( L ), float3( 1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f ) );\n"
	"    if ( g_Phong.x > 0.0f ) { float f = 1.0f - saturate( dot( bn, ev ) ); f *= f;\n"
	"      float fr = f > 0.5f ? lerp( g_PhongFresnel.y, g_PhongFresnel.z, 2.0f * f - 1.0f ) : lerp( g_PhongFresnel.x, g_PhongFresnel.y, 2.0f * f );\n"
	"      c.rgb = pow( pow( c.rgb, float3( 2.2f, 2.2f, 2.2f ) ) + spec * g_PhongTint.rgb * ( pm * fr * g_Phong.y ), float3( 1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f ) ); } }\n"
	"  if ( g_Detail.x > 5.5f && g_Detail.x < 7.5f ) c.rgb += g_Detail.w * g_DetailTex.Sample( g_WrapSampler, i.uv3.xy ).rgb;\n"
	"  if ( g_Char.x > 0.5f && g_Char.x < 2.5f ) { float4 b = g_Texture.Sample( g_Sampler, i.uv ); float4 ir = g_MaskTex.Sample( g_Sampler, saturate( i.eye.xy ) );\n"
	"    float3 e = ( g_Char.x < 1.5f ? lerp( b.rgb, ir.rgb, ir.a ) : ir.rgb ) * i.color.rgb;\n"
	"    if ( g_Char.x < 1.5f && g_Lightmap > 0.5f ) e = pow( pow( e, float3( 2.2f, 2.2f, 2.2f ) ) + g_LightmapTex.Sample( g_LightmapSampler, i.eye.zw ).rgb * g_Char.y, float3( 1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f ) );\n"
	"    c = float4( e, g_Char.x < 1.5f ? b.a * i.color.a : i.color.a ); }\n"
	"  if ( g_SelfIllum.w > 0.5f ) { float4 sb = g_Texture.Sample( g_Sampler, i.uv );\n"
	"    float sm = g_SelfIllum.w > 1.5f ? g_MaskTex.Sample( g_Sampler, i.uv ).r : sb.a;\n"
	"    c.rgb = lerp( c.rgb, sb.rgb * g_SelfIllum.rgb, sm ); c.a = i.color.a; }\n"
	"  if ( g_EyePos.w > 0.5f ) { float3 v = normalize( i.wpos - g_EyePos.xyz ); float3 rv = reflect( v, bn );\n"
	"    float4 m = g_MaskTex.Sample( g_Sampler, i.uv ); float4 b = g_Texture.Sample( g_Sampler, i.uv );\n"
	"    float3 mask = g_EnvTint.w > 4.5f ? float3( pm, pm, pm ) : ( g_EnvTint.w > 3.5f ? b.aaa : ( g_EnvTint.w > 2.5f ? m.aaa : ( g_EnvTint.w > 1.5f ? m.rgb : ( g_EnvTint.w > 0.5f ? 1.0f - b.aaa : float3( 1.0f, 1.0f, 1.0f ) ) ) ) );\n"
	"    if ( g_EnvParams.w > 0.5f ) { float f = 1.0f - saturate( dot( bn, -v ) ); f *= f;\n"
	"      mask *= lerp( 1.0f, f > 0.5f ? lerp( g_PhongFresnel.y, g_PhongFresnel.z, 2.0f * f - 1.0f ) : lerp( g_PhongFresnel.x, g_PhongFresnel.y, 2.0f * f ), g_EnvParams.z ); }\n"
	"    float3 spec = g_Envmap.Sample( g_EnvSampler, rv ).rgb * g_EnvTint.rgb * mask; spec = lerp( spec, spec * spec, g_EnvParams.x );\n"
	"    float grey = dot( spec, float3( 0.299f, 0.587f, 0.114f ) ); c.rgb += lerp( float3( grey, grey, grey ), spec, g_EnvParams.y ); }\n"
	"  if ( g_Water.x > 0.5f ) { float2 s = i.pos.xy * g_Water.yz; float2 wn = float2( 0.0f, 0.0f );\n"
	"    if ( g_Bump.x > 2.5f ) { float4 wm = g_BumpTex.Sample( g_WrapSampler, i.uv ); wn = ( wm.xy * 2.0f - 1.0f ) * wm.a; }\n"
	"    float3 refr = g_Texture.Sample( g_Sampler, s + wn * g_Bump.z ).rgb * g_RefractTint.rgb;\n"
	"    float3 refl = g_LightmapTex.Sample( g_LightmapSampler, float2( s.x, 1.0f - s.y ) + wn * g_Bump.w ).rgb * g_ReflectTint.rgb;\n"
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
static int s_hEnvCube;			// t2: the cube map
static float s_EnvTint[4], s_EyePos[4];
static float s_EnvParams[4] = { 0.0f, 1.0f, 0.0f, 0.0f };

// after SetEnvmap: $envmapcontrast (0 as is, 1 squared) and $envmapsaturation (0 grey, 1 as is)
void Dx12R_SetEnvmapParams( float flContrast, float flSaturation )
{
	s_EnvParams[0] = flContrast;
	s_EnvParams[1] = flSaturation;
}

// after SetEnvmap: the cube's texture, and the skin shader's envmap fresnel ($envmapfresnel through $phongfresnelranges)
void Dx12R_SetEnvmapCube( int hCube, bool bSkin, float flFresnel )
{
	s_hEnvCube = hCube;
	s_EnvParams[2] = flFresnel;
	s_EnvParams[3] = bSkin ? 1.0f : 0.0f;
}

void Dx12R_SetEnvmap( bool bEnable, const float *pTint, int nMask, const float *pEyePos, int hMask )
{
	s_bEnvmap = bEnable;
	s_hEnvMask = bEnable ? hMask : 0;
	s_hEnvCube = 0;
	s_EnvParams[2] = s_EnvParams[3] = 0.0f;
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

void Dx12R_SetStaticLight( bool bStatic )
{
	s_bStaticLight = bStatic;
}
static bool s_bShadowPass;


static bool s_bWaterPass;
static float s_ClipPlanes[2][4] = { { 0, 0, 0, -1 }, { 0, 0, 0, -1 } };

void Dx12R_SetClipPlane( int nIndex, const float *pPlane, bool bEnable )
{
	if ( nIndex < 0 || nIndex >= 2 )
		return;
	static const float s_Off[4] = { 0, 0, 0, -1 };
	memcpy( s_ClipPlanes[nIndex], ( bEnable && pPlane ) ? pPlane : s_Off, sizeof( s_Off ) );
}
static int s_hWaterReflection;
static float s_RefractTint[4] = { 1, 1, 1, 1 }, s_ReflectTint[4] = { 1, 1, 1, 1 };

void Dx12R_SetWaterPass( bool bWater, int hReflection, const float *pRefractTint, const float *pReflectTint )
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

void Dx12R_SetVertexColorUse( bool bColor, bool bAlpha, const float *pColor )
{
	s_VColorUse[0] = bColor ? 1.0f : 0.0f;
	s_VColorUse[1] = bAlpha ? 1.0f : 0.0f;
	for ( int i = 0; i < 4; ++i )
		s_MatColor[i] = pColor ? pColor[i] : 1.0f;
}

void Dx12R_SetBoneMatrices( const float ( *pBones )[12], int nCount )
{
	if ( nCount > DX12_MAX_BONES ) nCount = DX12_MAX_BONES;
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
void Dx12R_SetSelfIllum( int nMask, const float *pTint, int hMask )
{
	for ( int i = 0; i < 3; ++i )
		s_SelfIllum[i] = pTint ? pTint[i] : 1.0f;
	s_SelfIllum[3] = (float)nMask;
	if ( nMask == 2 )
		s_hEnvMask = hMask;
}
static float s_Char[4], s_CharConst[6][4];
static Dx12R_Surface_t s_PassSurface = { 0, false, 0, 0, 0, { 4.0f, 4.0f }, 1.0f, 0, { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f } } };

void Dx12R_SetSurface( const Dx12R_Surface_t &surface )
{
	s_PassSurface = surface;
}

// Eyes / EyeRefract / Teeth (see dx12render.h): the iris goes on t3 and the eye glint on t1
void Dx12R_SetCharacterPass( int nMode, const float ( *pConsts )[4], float flGlintDamping, int hIris, int hGlint )
{
	s_Char[0] = (float)nMode;
	s_Char[1] = flGlintDamping;
	if ( nMode && pConsts )
		memcpy( s_CharConst, pConsts, sizeof( s_CharConst ) );
	if ( nMode == 1 || nMode == 2 )
	{
		s_hEnvMask = hIris;
		s_hLightmap = nMode == 1 ? hGlint : 0;
	}
}
static float s_SceneFog[4], s_SceneFogColor[3];	// see Dx12R_SetSceneFog

void Dx12R_SetSceneFog( bool bEnable, float flStart, float flEnd, float flMaxDensity, const unsigned char *pColor )
{
	s_SceneFog[0] = flStart;
	s_SceneFog[1] = flEnd > flStart ? 1.0f / ( flEnd - flStart ) : 0.0f;
	s_SceneFog[2] = flMaxDensity;
	s_SceneFog[3] = ( bEnable && flEnd > flStart ) ? 1.0f : 0.0f;
	for ( int i = 0; i < 3; ++i )
		s_SceneFogColor[i] = pColor ? pColor[i] / 255.0f : 0.0f;
}

// Source's LDR bloom passes (see SetPostPass in dx12render.h)
void Dx12R_SetPostPass( int nMode, const float *pParams, int nTexWidth, int nTexHeight )
{
	s_Post[0] = (float)nMode;
	s_Post[2] = nTexWidth > 0 ? 1.0f / nTexWidth : 0.0f;
	s_Post[3] = nTexHeight > 0 ? 1.0f / nTexHeight : 0.0f;
	for ( int i = 0; i < 4; ++i )
		s_PostParams[i] = pParams ? pParams[i] : 1.0f;
}

void Dx12R_SetShadowPass( bool bShadow, const float *pShadowColor )
{
	s_bShadowPass = bShadow;
	if ( bShadow && pShadowColor )
		memcpy( s_ShadowColor, pShadowColor, sizeof( s_ShadowColor ) );
}
static float s_Model[16];
static float s_ViewEye[3];
static float s_Ambient[6][3];
static int s_nLights;
static Dx12Light_t s_Lights[4];

void Dx12R_SetLighting( bool bLit, const float ambientCube[6][3], int nLights, const Dx12Light_t *pLights )
{
	static int s_nNoLit = -1;
	if ( s_nNoLit < 0 ) { char b[8]; s_nNoLit = GetEnvironmentVariableA( "GAMMA_DX12_NOLIT", b, 8 ) ? 1 : 0; }
	s_bLit = bLit && !s_nNoLit;
	if ( !s_bLit )
		return;
	memcpy( s_Ambient, ambientCube, sizeof( s_Ambient ) );
	s_nLights = nLights < 4 ? nLights : 4;
	for ( int i = 0; i < s_nLights; ++i )
		s_Lights[i] = pLights[i];
}

void Dx12R_BindLightmap( int hTexture )
{
	s_hLightmap = hTexture;
}

void Dx12R_BindTexture( int nStage, int hTexture )
{
	extern int g_nDx12Binds;
	if ( hTexture > 0 ) ++g_nDx12Binds;
	if ( nStage == 0 )
		s_hBound = hTexture;
	if ( nStage >= 0 && nStage < 16 )
		s_hStages[nStage] = hTexture;
}

int Dx12R_GetBoundTexture( int nStage )
{
	return nStage == 0 ? s_hBound : ( nStage > 0 && nStage < 16 ) ? s_hStages[nStage] : 0;
}

//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------
void Dx12R_SetState( const Dx12State_t &state )
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

void Dx12R_SetMatrices( const float *pModel, const float *pView, const float *pProj )
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
	memcpy( pConst->m_Char, s_Char, sizeof( s_Char ) );
	memcpy( pConst->m_CharConst, s_CharConst, sizeof( s_CharConst ) );
	const Dx12R_Surface_t &surf = s_PassSurface;
	pConst->m_Bump[0] = ( surf.m_nBump && surf.m_hBump ) ? (float)surf.m_nBump : 0.0f;
	pConst->m_Bump[1] = surf.m_bSSBump ? 1.0f : 0.0f;
	pConst->m_Bump[2] = surf.m_flWater[0];
	pConst->m_Bump[3] = surf.m_flWater[1];
	pConst->m_Detail[0] = surf.m_hDetail ? (float)surf.m_nDetail : 0.0f;
	pConst->m_Detail[1] = surf.m_flDetailScale[0];
	pConst->m_Detail[2] = surf.m_flDetailScale[1];
	pConst->m_Detail[3] = surf.m_flDetailFactor;
	pConst->m_Base2[0] = surf.m_hBase2 ? 1.0f : 0.0f;
	pConst->m_Base2[1] = pConst->m_Base2[2] = pConst->m_Base2[3] = 0.0f;
	memcpy( pConst->m_BaseTx, surf.m_BaseTransform, sizeof( pConst->m_BaseTx ) );
	memcpy( pConst->m_Phong, surf.m_Phong, sizeof( pConst->m_Phong ) );
	memcpy( pConst->m_PhongFresnel, surf.m_PhongFresnel, sizeof( pConst->m_PhongFresnel ) );
	memcpy( pConst->m_PhongTint, surf.m_PhongTint, sizeof( pConst->m_PhongTint ) );
	memcpy( pConst->m_ViewEye, s_ViewEye, sizeof( s_ViewEye ) );
	pConst->m_ViewEye[3] = 1.0f;
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
			const Dx12Light_t &l = s_Lights[i];
			memcpy( pConst->m_LightColor[i], l.m_Color, 12 ); pConst->m_LightColor[i][3] = (float)l.m_nType;
			memcpy( pConst->m_LightPos[i], l.m_Pos, 12 ); pConst->m_LightPos[i][3] = l.m_flRange;
			memcpy( pConst->m_LightDir[i], l.m_Dir, 12 );
			memcpy( pConst->m_LightAtten[i], l.m_flAtten, 12 ); pConst->m_LightAtten[i][3] = l.m_flFalloff;
			pConst->m_LightCone[i][0] = l.m_flThetaDot; pConst->m_LightCone[i][1] = l.m_flPhiDot;
		}
	}
}
