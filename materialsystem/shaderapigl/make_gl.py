"""Generate shaderapigl's Source-facing files from the DX11 backend, so the native backends share
one implementation of meshes, snapshots, command buffers and textures, and write glshaders.h:
the DX11 backend's HLSL (rendershared.inl) rewritten as GLSL 4.50.
Usage: py -3 materialsystem/shaderapigl/make_gl.py materialsystem  (from the repo root)"""
import sys, os, re
M = sys.argv[1]
dx = os.path.join(M, 'shaderapidx11'); gl = os.path.join(M, 'shaderapigl')

RENAMES = [('D3D11 compiler', 'D3D compiler'), ('GAMMA_DX11_', 'GAMMA_GL_'), ('Dx11R_', 'GlR_'), ('Dx11_', 'Gl_'),
           ('Dx11', 'GlR'), ('DX11', 'GLR'), ('dx11', 'gl'), ('Direct3D 11', 'OpenGL')]


def convert(text):
    for a, b in RENAMES:
        text = text.replace(a, b)
    return text


HEADER = """//========= GammaSource ============//
//
// GENERATED from materialsystem/shaderapidx11 by materialsystem/shaderapigl/make_gl.py - edit the
// DX11 files and regenerate, so the backends stay one implementation.
//
//=============================================================================//
"""

for src, dst in [('shaderapidx11.cpp', 'shaderapigl.cpp'), ('rendershared.inl', 'rendershared.inl'), ('dx11render.h', 'glrender.h')]:
    text = convert(open(os.path.join(dx, src)).read())
    assert 'Dx11' not in text and 'D3D11' not in text, [l for l in text.splitlines() if 'Dx11' in l or 'D3D11' in l][:5]
    open(os.path.join(gl, dst), 'w').write(HEADER + text)

# ---- the shader: HLSL -> GLSL ----
src = open(os.path.join(dx, 'rendershared.inl')).read()
body = src[src.index('static const char s_szShader[] ='):]
body = body[:body.index(';\n')]
body = body.replace('GAMMA_SHADER_BINDINGS', '')
hlsl = ''.join(m.encode('latin-1').decode('unicode_escape') for m in re.findall(r'"((?:[^"\\]|\\.)*)"', body))


def sub(old, new, text, count=1):
    """exact rewrite that must match: a changed HLSL shader fails here instead of silently"""
    assert text.count(old) == count, (old, text.count(old))
    return text.replace(old, new)


g = hlsl
g = re.sub(r'VKB\( \d+ \) ', '', g)
# resources
g = sub('cbuffer Constants : register( b0 ) {', 'layout( std140, binding = 0 ) uniform Constants {', g)
g = sub('cbuffer Bones : register( b1 ) {', 'layout( std140, binding = 1 ) uniform Bones {', g)
g = g.replace('row_major float4x4', 'layout( row_major ) float4x4')
g = sub('Texture2D g_Texture : register( t0 ); SamplerState g_Sampler : register( s0 );',
        'layout( binding = 0 ) uniform sampler2D g_Texture;', g)
g = sub('Texture2D g_LightmapTex : register( t1 ); SamplerState g_LightmapSampler : register( s1 );',
        'layout( binding = 1 ) uniform sampler2D g_LightmapTex;', g)
g = sub('TextureCube g_Envmap : register( t2 ); SamplerState g_EnvSampler : register( s2 );',
        'layout( binding = 2 ) uniform samplerCube g_Envmap;', g)
g = sub('Texture2D g_MaskTex : register( t3 );', 'layout( binding = 3 ) uniform sampler2D g_MaskTex;', g)
g = sub('Texture2D g_BumpTex : register( t4 ); Texture2D g_DetailTex : register( t5 ); Texture2D g_Base2Tex : register( t6 );',
        'layout( binding = 4 ) uniform sampler2D g_BumpTex; layout( binding = 5 ) uniform sampler2D g_DetailTex; layout( binding = 6 ) uniform sampler2D g_Base2Tex;', g)
g = sub('SamplerState g_WrapSampler : register( s3 );', '', g)
g = re.sub(r'(\w+)\.Sample\( \w+, ', r'texture( \1, ', g)
# semantics go; the entry points become plain functions called from main()
g = re.sub(r'\s*:\s*(SV_\w+|POSITION|COLOR|TEXCOORD\d|NORMAL|BLENDINDICES|BLENDWEIGHT|TANGENT|BINORMAL)\b', '', g)
g = sub('uint4 bones', 'int4 bones', g)
g = sub('[unroll] ', '', g)
# what GLSL types or overloads differently
g = sub('n < 0.0f ? int3( 1, 3, 5 ) : int3( 0, 2, 4 )', 'mix( int3( 0, 2, 4 ), int3( 1, 3, 5 ), lessThan( n, float3( 0.0f ) ) )', g)
g = sub('lerp( 1.0f, v.color, g_VColorUse.xxxy )', 'lerp( float4( 1.0f ), v.color, g_VColorUse.xxxy )', g)
g = sub('(float3x3)g_Model', 'float3x3( g_Model )', g, 3)
# the vertex and pixel halves
vs_at = g.index('PSIn VSMain(')
ps_at = g.index('float4 PSMain(')
g = g[:vs_at] + '#ifdef GAMMA_VS\n' + g[vs_at:ps_at] + '#endif\n#ifdef GAMMA_PS\n' + g[ps_at:] + '#endif\n'

PRELUDE = r"""#define float2 vec2
#define float3 vec3
#define float4 vec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define float3x3 mat3
#define float4x4 mat4
#define lerp mix
#define saturate( x ) clamp( x, 0.0f, 1.0f )
#define mul( a, b ) ( ( a ) * ( b ) )
#define clip( x ) if ( ( x ) < 0.0f ) discard
"""

# Direct3D conventions are kept: render targets hold rows top-down (y is flipped into clip
# space here, and back when the back buffer is shown), depth is 0..1 (glClipControl)
MAINS = r"""#ifdef GAMMA_VS
layout( location = 0 ) in vec3 a_pos;
layout( location = 1 ) in vec4 a_color;
layout( location = 2 ) in vec2 a_uv;
layout( location = 3 ) in vec2 a_uv2;
layout( location = 4 ) in vec3 a_normal;
layout( location = 5 ) in ivec4 a_bones;
layout( location = 6 ) in vec2 a_weights;
layout( location = 7 ) in vec3 a_tangentS;
layout( location = 8 ) in vec3 a_tangentT;
layout( location = 9 ) in float a_lmofs;
out vec4 v_color; out vec2 v_uv; out vec2 v_uv2; out vec3 v_wpos; out vec3 v_wnorm; out float v_fogz; out vec4 v_eye;
out vec3 v_tan; out vec4 v_bin; out vec4 v_uv3;
void main() { VSIn v; v.pos = a_pos; v.color = a_color; v.uv = a_uv; v.uv2 = a_uv2; v.normal = a_normal; v.bones = a_bones; v.weights = a_weights;
  v.tangentS = a_tangentS; v.tangentT = a_tangentT; v.lmofs = a_lmofs;
  PSIn o = VSMain( v );
  gl_Position = vec4( o.pos.x, -o.pos.y, o.pos.z, o.pos.w );
  gl_ClipDistance[0] = o.clip.x; gl_ClipDistance[1] = o.clip.y;
  v_color = o.color; v_uv = o.uv; v_uv2 = o.uv2; v_wpos = o.wpos; v_wnorm = o.wnorm; v_fogz = o.fogz; v_eye = o.eye; v_tan = o.tan; v_bin = o.bin; v_uv3 = o.uv3; }
#endif
#ifdef GAMMA_PS
in vec4 v_color; in vec2 v_uv; in vec2 v_uv2; in vec3 v_wpos; in vec3 v_wnorm; in float v_fogz; in vec4 v_eye;
in vec3 v_tan; in vec4 v_bin; in vec4 v_uv3;
layout( location = 0 ) out vec4 o_target;
void main() { PSIn i; i.pos = gl_FragCoord; i.color = v_color; i.uv = v_uv; i.uv2 = v_uv2; i.clip = vec2( 0.0f ); i.wpos = v_wpos; i.wnorm = v_wnorm; i.fogz = v_fogz; i.eye = v_eye; i.tan = v_tan; i.bin = v_bin; i.uv3 = v_uv3;
  o_target = PSMain( i ); }
#endif
"""
glsl = PRELUDE + g + MAINS


def c_string(text):
    out = []
    for line in text.split('\n')[:-1] if text.endswith('\n') else text.split('\n'):
        out.append('\t"' + line.replace('\\', '\\\\').replace('"', '\\"') + '\\n"')
    return '\n'.join(out)


open(os.path.join(gl, 'glshaders.h'), 'w').write(
    '//========= GammaSource ============//\n//\n// GENERATED by materialsystem/shaderapigl/make_gl.py from rendershared.inl\'s HLSL (the DX11\n'
    '// backend\'s shader), rewritten as GLSL. glrender.cpp puts "#version 450 core" and GAMMA_VS or\n'
    '// GAMMA_PS in front of it.\n//\n//=============================================================================//\n'
    '#ifndef GLSHADERS_H\n#define GLSHADERS_H\n#pragma once\n\nstatic const char s_szGLShader[] =\n' + c_string(glsl) + ';\n\n#endif // GLSHADERS_H\n')
print('ok')
