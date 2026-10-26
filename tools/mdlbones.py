"""Print bones (index, parent, name) and texture/bodypart names of a v44-v49 MDL."""
import struct, sys

def cstr(b, o):
    e = b.index(b'\0', o)
    return b[o:e].decode('latin1')

b = open(sys.argv[1], 'rb').read()
ver = struct.unpack_from('<i', b, 4)[0]
# id, version, checksum, name[64], length, 6 vectors, flags, numbones, boneindex
o = 12 + 64 + 4 + 72
flags, numbones, boneindex = struct.unpack_from('<iii', b, o)
print('version', ver, 'bones', numbones)
BONE = 216 if ver >= 44 else None
for i in range(numbones):
    bo = boneindex + i * BONE
    nameidx, parent = struct.unpack_from('<ii', b, bo)
    print(i, parent, cstr(b, bo + nameidx))
# textures: after bonecontrollers, hitbox sets, localanim, localseq, activitylistversion, eventsindexed
o2 = o + 12
vals = struct.unpack_from('<' + 'i' * 20, b, o2)
# numbonecontrollers, bonecontrollerindex, numhitboxsets, hitboxsetindex, numlocalanim, localanimindex,
# numlocalseq, localseqindex, activitylistversion, eventsindexed, numtextures, textureindex, numcdtextures, cdtextureindex, ...
numtex, texidx = vals[10], vals[11]
for i in range(numtex):
    to = texidx + i * 64
    print('tex', cstr(b, to + struct.unpack_from('<i', b, to)[0]))
numcd, cdidx = vals[12], vals[13]
for i in range(numcd):
    print('cd', cstr(b, struct.unpack_from('<i', b, cdidx + i * 4)[0]))
