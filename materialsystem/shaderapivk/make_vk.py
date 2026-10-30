"""Generate shaderapivk's Source-facing files from the DX11 backend, so both share one
implementation of meshes, snapshots, command buffers and textures.
Usage: py -3 materialsystem/shaderapivk/make_vk.py materialsystem  (from the repo root)"""
import sys, os, re
M = sys.argv[1]
dx = os.path.join(M, 'shaderapidx11'); vk = os.path.join(M, 'shaderapivk')

RENAMES = [
    ('Dx11R_', 'VkR_'), ('Dx11_', 'Vk_'),
    ('Dx11Vertex_t', 'VkRVertex_t'), ('Dx11State_t', 'VkRState_t'), ('Dx11Primitive_t', 'VkRPrimitive_t'),
    ('Dx11Snapshot_t', 'VkRSnapshot_t'), ('g_nDx11', 'g_nVk'),
    ('DX11_POINTS', 'VKR_POINTS'), ('DX11_LINES', 'VKR_LINES'), ('DX11_TRIANGLES', 'VKR_TRIANGLES'),
    ('DX11_TRIANGLE_STRIP', 'VKR_TRIANGLE_STRIP'), ('DX11_LINE_STRIP', 'VKR_LINE_STRIP'),
    ('"dx11device.h"', '"vkdevice.h"'), ('"dx11render.h"', '"vkrender.h"'),
    ('DX11RENDER_H', 'VKRENDER_H'),
]


def convert(text):
    for a, b in RENAMES:
        text = text.replace(a, b)
    return text


HEADER = """//========= GammaSource ============//
//
// GENERATED from materialsystem/shaderapidx11 by materialsystem/shaderapivk/make_vk.py - edit the DX11
// files and regenerate, so the two backends stay one implementation.
//
//=============================================================================//
"""

src = open(os.path.join(dx, 'shaderapidx11.cpp')).read()
out = convert(src)
# the shaderapi talks to the device only through the Vk_/VkR_ functions
assert 'Dx11' not in out and 'DX11_POINTS' not in out, [l for l in out.splitlines() if 'Dx11' in l][:5]
open(os.path.join(vk, 'shaderapivk.cpp'), 'w').write(HEADER + out)

h = open(os.path.join(dx, 'dx11render.h')).read()
h = convert(h).replace('Native Direct3D 11 drawing for shaderapidx11', 'Native Vulkan drawing for shaderapivk')
h = h.replace('enum VkRPrimitive_t\n{\n\tVKR_POINTS', 'enum VkRPrimitive_t\n{\n\tVKR_POINTS')
open(os.path.join(vk, 'vkrender.h'), 'w').write(HEADER + h)
print('ok')
