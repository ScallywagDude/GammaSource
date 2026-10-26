#!/usr/bin/env python3
"""Conservative numerical repair for the existing DX9 cheap-water shader.

No new material assets or modern API backend are implied by this overlay.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'materialsystem/stdshaders/WaterCheap_ps20.fxc'
PATCHES = (
    ('HALF  worldSpaceDistSqr = dot( worldSpaceEye, worldSpaceEye );',
     '// HL2MODERN_WATER_SAFE_NORMALIZE: floor is representable in half precision.\n'
     '\t\tHALF  worldSpaceDistSqr = max( dot( worldSpaceEye, worldSpaceEye ), 0.0001f );'),
    ('worldSpaceDist   = length( worldSpaceEye );',
     'worldSpaceDist   = max( length( worldSpaceEye ), 0.01f );'),
    ('dotResult = max( 0.0, dotResult );',
     '// HL2MODERN_WATER_BOUNDED_FRESNEL: compressed normals may overshoot one.\n'
     '\tdotResult = saturate( dotResult );'),
)


def main():
    raw=SOURCE.read_bytes()
    crlf=b'\r\n' in raw
    text=raw.decode('latin1').replace('\r\n','\n')
    for old,new in PATCHES:
        if new in text:continue
        if text.count(old)!=1:raise RuntimeError(f'{SOURCE.name}: shader patch site mismatch')
        text=text.replace(old,new)
    SOURCE.write_bytes(text.replace('\n','\r\n').encode('latin1') if crlf else text.encode('latin1'))
    print('DX9 cheap-water normalization/Fresnel overlay applied (or already present)')


if __name__=='__main__':main()
