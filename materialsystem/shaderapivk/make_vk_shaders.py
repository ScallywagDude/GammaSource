"""Writes materialsystem/shaderapivk/vkshaders.h: the SPIR-V for shaderapivk's two shaders.

No SPIR-V compiler ships with the Windows SDK, so the modules are assembled here directly.
They match this GLSL (the same as the DX11 backend's HLSL):

  layout(push_constant) uniform PC { mat4 wvp; float alphaRef, alphaTest, lightmap, pad; } pc;
  // vertex
  layout(location=0) in vec3 pos; layout(location=1) in vec4 color;
  layout(location=2) in vec2 uv;  layout(location=3) in vec2 uv2;
  void main() { gl_Position = pc.wvp * vec4(pos, 1); vColor = color; vUV = uv; vUV2 = uv2; }
  // fragment
  layout(binding=0) uniform sampler2D tex; layout(binding=1) uniform sampler2D lightmapTex;
  void main() {
      vec4 c = texture(tex, vUV) * vColor;
      vec4 lm = vec4(texture(lightmapTex, vUV2).rgb * 2, 1);
      c *= 1 + (lm - 1) * pc.lightmap;                 // lightmap is 0 or 1
      if (pc.alphaTest > 0.5 && c.a < pc.alphaRef) discard;
      outColor = c;
  }

The matrix arrives row-major (Direct3D row vectors); read as column-major it is the
transpose, so wvp * v gives the Direct3D result.

Usage: py -3 materialsystem/shaderapivk/make_vk_shaders.py
"""
import os, struct

OP = dict(Capability=17, MemoryModel=14, EntryPoint=15, ExecutionMode=16, Decorate=71, MemberDecorate=72,
          TypeVoid=19, TypeBool=20, TypeInt=21, TypeFloat=22, TypeVector=23, TypeMatrix=24, TypeImage=25,
          TypeSampledImage=27, TypeStruct=30, TypePointer=32, TypeFunction=33, Constant=43, ConstantComposite=44,
          Function=54, FunctionEnd=56, Variable=59, Load=61, Store=62, AccessChain=65, VectorShuffle=79,
          CompositeConstruct=80, CompositeExtract=81, ImageSampleImplicitLod=87, FAdd=129, FSub=131, FMul=133,
          VectorTimesScalar=142, MatrixTimesVector=145, LogicalAnd=167, FOrdLessThan=184, FOrdGreaterThan=186,
          SelectionMerge=247, Label=248, Branch=249, BranchConditional=250, Kill=252, Return=253)
# enums
SHADER = 1; LOGICAL = 0; GLSL450 = 1; VERTEX = 0; FRAGMENT = 4; ORIGIN_UPPER_LEFT = 7
SC_UNIFORM_CONSTANT = 0; SC_INPUT = 1; SC_OUTPUT = 3; SC_PUSH_CONSTANT = 9; SC_FUNCTION = 7
D_BLOCK = 2; D_COLMAJOR = 5; D_MATRIXSTRIDE = 7; D_BUILTIN = 11; D_LOCATION = 30; D_BINDING = 33; D_DESCRIPTORSET = 34; D_OFFSET = 35
BUILTIN_POSITION = 0; DIM_2D = 1


def string(s):
    b = s.encode() + b'\0'
    b += b'\0' * ((4 - len(b) % 4) % 4)
    return list(struct.unpack('<%dI' % (len(b) // 4), b))


def f32(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Module:
    def __init__(self):
        self.words = []

    def op(self, name, *args):
        flat = []
        for a in args:
            flat.extend(a if isinstance(a, list) else [a])
        self.words.append(((len(flat) + 1) << 16) | OP[name])
        self.words.extend(flat)

    def binary(self, bound):
        return [0x07230203, 0x00010000, 0, bound, 0] + self.words


def push_constant_block(m, mat4, flt, struct_id):
    m.op('Decorate', struct_id, D_BLOCK)
    m.op('MemberDecorate', struct_id, 0, D_COLMAJOR)
    m.op('MemberDecorate', struct_id, 0, D_OFFSET, 0)
    m.op('MemberDecorate', struct_id, 0, D_MATRIXSTRIDE, 16)
    for i in range(1, 5):
        m.op('MemberDecorate', struct_id, i, D_OFFSET, 64 + (i - 1) * 4)


def vertex():
    m = Module()
    m.op('Capability', SHADER)
    m.op('MemoryModel', LOGICAL, GLSL450)
    m.op('EntryPoint', VERTEX, 28, string('main'), 15, 17, 19, 20, 22, 23, 25, 26)
    push_constant_block(m, 7, 3, 8)
    for var, loc in ((15, 0), (17, 1), (19, 2), (20, 3), (23, 0), (25, 1), (26, 2)):
        m.op('Decorate', var, D_LOCATION, loc)
    m.op('Decorate', 22, D_BUILTIN, BUILTIN_POSITION)
    m.op('TypeVoid', 1)
    m.op('TypeFunction', 2, 1)
    m.op('TypeFloat', 3, 32)
    m.op('TypeVector', 4, 3, 4)
    m.op('TypeVector', 5, 3, 3)
    m.op('TypeVector', 6, 3, 2)
    m.op('TypeMatrix', 7, 4, 4)
    m.op('TypeStruct', 8, 7, 3, 3, 3, 3)
    m.op('TypePointer', 9, SC_PUSH_CONSTANT, 8)
    m.op('Variable', 9, 10, SC_PUSH_CONSTANT)
    m.op('TypeInt', 11, 32, 1)
    m.op('Constant', 11, 12, 0)
    m.op('TypePointer', 13, SC_PUSH_CONSTANT, 7)
    m.op('TypePointer', 14, SC_INPUT, 5)
    m.op('Variable', 14, 15, SC_INPUT)
    m.op('TypePointer', 16, SC_INPUT, 4)
    m.op('Variable', 16, 17, SC_INPUT)
    m.op('TypePointer', 18, SC_INPUT, 6)
    m.op('Variable', 18, 19, SC_INPUT)
    m.op('Variable', 18, 20, SC_INPUT)
    m.op('TypePointer', 21, SC_OUTPUT, 4)
    m.op('Variable', 21, 22, SC_OUTPUT)
    m.op('Variable', 21, 23, SC_OUTPUT)
    m.op('TypePointer', 24, SC_OUTPUT, 6)
    m.op('Variable', 24, 25, SC_OUTPUT)
    m.op('Variable', 24, 26, SC_OUTPUT)
    m.op('Constant', 3, 27, f32(1.0))
    m.op('Function', 1, 28, 0, 2)
    m.op('Label', 29)
    m.op('AccessChain', 13, 30, 10, 12)
    m.op('Load', 7, 31, 30)
    m.op('Load', 5, 32, 15)
    m.op('CompositeExtract', 3, 33, 32, 0)
    m.op('CompositeExtract', 3, 34, 32, 1)
    m.op('CompositeExtract', 3, 35, 32, 2)
    m.op('CompositeConstruct', 4, 36, 33, 34, 35, 27)
    m.op('MatrixTimesVector', 4, 37, 31, 36)
    m.op('Store', 22, 37)
    m.op('Load', 4, 38, 17)
    m.op('Store', 23, 38)
    m.op('Load', 6, 39, 19)
    m.op('Store', 25, 39)
    m.op('Load', 6, 40, 20)
    m.op('Store', 26, 40)
    m.op('Return')
    m.op('FunctionEnd')
    return m.binary(41)


def fragment():
    m = Module()
    m.op('Capability', SHADER)
    m.op('MemoryModel', LOGICAL, GLSL450)
    m.op('EntryPoint', FRAGMENT, 31, string('main'), 18, 20, 21, 23)
    m.op('ExecutionMode', 31, ORIGIN_UPPER_LEFT)
    push_constant_block(m, 7, 3, 8)
    m.op('Decorate', 15, D_DESCRIPTORSET, 0)
    m.op('Decorate', 15, D_BINDING, 0)
    m.op('Decorate', 16, D_DESCRIPTORSET, 0)
    m.op('Decorate', 16, D_BINDING, 1)
    for var, loc in ((18, 0), (20, 1), (21, 2), (23, 0)):
        m.op('Decorate', var, D_LOCATION, loc)
    m.op('TypeVoid', 1)
    m.op('TypeFunction', 2, 1)
    m.op('TypeFloat', 3, 32)
    m.op('TypeVector', 4, 3, 4)
    m.op('TypeVector', 5, 3, 2)
    m.op('TypeMatrix', 7, 4, 4)
    m.op('TypeStruct', 8, 7, 3, 3, 3, 3)
    m.op('TypePointer', 9, SC_PUSH_CONSTANT, 8)
    m.op('Variable', 9, 10, SC_PUSH_CONSTANT)
    m.op('TypeInt', 11, 32, 1)
    m.op('TypeImage', 12, 3, DIM_2D, 0, 0, 0, 1, 0)
    m.op('TypeSampledImage', 13, 12)
    m.op('TypePointer', 14, SC_UNIFORM_CONSTANT, 13)
    m.op('Variable', 14, 15, SC_UNIFORM_CONSTANT)
    m.op('Variable', 14, 16, SC_UNIFORM_CONSTANT)
    m.op('TypePointer', 17, SC_INPUT, 4)
    m.op('Variable', 17, 18, SC_INPUT)
    m.op('TypePointer', 19, SC_INPUT, 5)
    m.op('Variable', 19, 20, SC_INPUT)
    m.op('Variable', 19, 21, SC_INPUT)
    m.op('TypePointer', 22, SC_OUTPUT, 4)
    m.op('Variable', 22, 23, SC_OUTPUT)
    m.op('TypePointer', 24, SC_PUSH_CONSTANT, 3)
    m.op('Constant', 11, 25, 3)		# lightmap
    m.op('Constant', 11, 26, 2)		# alphaTest
    m.op('Constant', 11, 27, 1)		# alphaRef
    m.op('Constant', 3, 28, f32(0.5))
    m.op('Constant', 3, 29, f32(2.0))
    m.op('TypeBool', 30)
    m.op('Constant', 3, 66, f32(1.0))
    m.op('ConstantComposite', 4, 67, 66, 66, 66, 66)
    m.op('Function', 1, 31, 0, 2)
    m.op('Label', 32)
    m.op('Load', 13, 40, 15)
    m.op('Load', 5, 41, 20)
    m.op('ImageSampleImplicitLod', 4, 42, 40, 41)
    m.op('Load', 4, 43, 18)
    m.op('FMul', 4, 44, 42, 43)
    m.op('Load', 13, 45, 16)
    m.op('Load', 5, 46, 21)
    m.op('ImageSampleImplicitLod', 4, 47, 45, 46)
    m.op('VectorTimesScalar', 4, 48, 47, 29)
    m.op('VectorShuffle', 4, 49, 48, 67, 0, 1, 2, 7)
    m.op('AccessChain', 24, 50, 10, 25)
    m.op('Load', 3, 51, 50)
    m.op('FSub', 4, 52, 49, 67)
    m.op('VectorTimesScalar', 4, 53, 52, 51)
    m.op('FAdd', 4, 54, 53, 67)
    m.op('FMul', 4, 55, 44, 54)
    m.op('AccessChain', 24, 56, 10, 26)
    m.op('Load', 3, 57, 56)
    m.op('FOrdGreaterThan', 30, 58, 57, 28)
    m.op('CompositeExtract', 3, 59, 55, 3)
    m.op('AccessChain', 24, 60, 10, 27)
    m.op('Load', 3, 61, 60)
    m.op('FOrdLessThan', 30, 62, 59, 61)
    m.op('LogicalAnd', 30, 63, 58, 62)
    m.op('SelectionMerge', 65, 0)
    m.op('BranchConditional', 63, 64, 65)
    m.op('Label', 64)
    m.op('Kill')
    m.op('Label', 65)
    m.op('Store', 23, 55)
    m.op('Return')
    m.op('FunctionEnd')
    return m.binary(68)


def emit(name, words):
    lines = []
    for i in range(0, len(words), 8):
        lines.append('\t' + ', '.join('0x%08x' % w for w in words[i:i + 8]) + ',')
    return 'static const unsigned int %s[] =\n{\n%s\n};\n' % (name, '\n'.join(lines))


here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, 'vkshaders.h')
text = ('//========= GammaSource ============//\n//\n// GENERATED by materialsystem/shaderapivk/make_vk_shaders.py: SPIR-V for shaderapivk\'s vertex and\n'
        '// fragment shaders (see that script for the GLSL they correspond to).\n//\n'
        '//=============================================================================//\n'
        '#ifndef VKSHADERS_H\n#define VKSHADERS_H\n#pragma once\n\n' +
        emit('g_VkVertexShader', vertex()) + '\n' + emit('g_VkFragmentShader', fragment()) + '\n#endif // VKSHADERS_H\n')
open(out, 'w').write(text)
print('wrote', os.path.normpath(out))
