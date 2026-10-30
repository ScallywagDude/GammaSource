"""Generate shaderapidx12's Source-facing files from the DX11 backend, so the native backends share
one implementation of meshes, snapshots, command buffers, textures and the shader.
Usage: py -3 materialsystem/shaderapidx12/make_dx12.py materialsystem  (from the repo root)"""
import sys, os
M = sys.argv[1]
dx = os.path.join(M, 'shaderapidx11'); d12 = os.path.join(M, 'shaderapidx12')

RENAMES = [('D3D11 compiler', 'D3D compiler'), ('Dx11', 'Dx12'), ('DX11', 'DX12'), ('dx11', 'dx12'), ('Direct3D 11', 'Direct3D 12')]


def convert(text):
    for a, b in RENAMES:
        text = text.replace(a, b)
    return text


HEADER = """//========= GammaSource ============//
//
// GENERATED from materialsystem/shaderapidx11 by materialsystem/shaderapidx12/make_dx12.py - edit the
// DX11 files and regenerate, so the backends stay one implementation.
//
//=============================================================================//
"""

for src, dst in [('shaderapidx11.cpp', 'shaderapidx12.cpp'), ('rendershared.inl', 'rendershared.inl'), ('dx11render.h', 'dx12render.h')]:
    text = convert(open(os.path.join(dx, src)).read())
    assert 'Dx11' not in text and 'D3D11' not in text, [l for l in text.splitlines() if 'Dx11' in l or 'D3D11' in l][:5]
    open(os.path.join(d12, dst), 'w').write(HEADER + text)
print('ok')
