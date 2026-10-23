#!/usr/bin/env python3
"""Conservatively rebuild compatible packed Source VTX version-7 data as version 6.

Reads only an authorized local input. This changes *vertex index metadata*, not
MDL bones, animations, VVD vertices, models, textures, or materials. V37 MDL
compatibility must still be established by the engine's studio validator.

Output is written to a NEW path only; never overwrites original game content.
Historical Source 2003 optimizedmodel v6 and Source 2005+ optimizedmodel v7
both use packed relative-offset trees; their Vertex_t layouts differ.
"""
import argparse
import pathlib
import struct

LIMIT = 64 * 1024 * 1024

class FormatError(ValueError):
    pass

class VtxReader:
    def __init__(self, data):
        if len(data) < 36 or len(data) > LIMIT:
            raise FormatError('VTX size outside bounds')
        self.data = data

    def unpack(self, fmt, offset):
        size = struct.calcsize('<' + fmt)
        if offset < 0 or size > len(self.data) - offset:
            raise FormatError('truncated VTX structure at offset %d' % offset)
        return struct.unpack_from('<' + fmt, self.data, offset)

    def table(self, owner, relative, count, stride, maximum):
        if not 0 <= count <= maximum:
            raise FormatError('unreasonable VTX table count')
        if not count:
            return owner + relative
        at = owner + relative
        if relative <= 0 or at < 0 or at > len(self.data) or count > (len(self.data) - at) // stride:
            raise FormatError('out-of-bounds relative VTX table')
        return at

    def string(self, owner, relative):
        pos = owner + relative
        if relative <= 0 or pos < 0 or pos >= len(self.data):
            raise FormatError('bad replacement material name offset')
        end = self.data.find(b'\x00', pos)
        if end == -1 or end - pos > 4096:
            raise FormatError('unterminated replacement material name')
        return self.data[pos:end + 1]

class VtxBuilder:
    def __init__(self):
        self.data = bytearray(36)

    def reserve(self, length):
        if length < 0 or len(self.data) + length > LIMIT:
            raise FormatError('converted VTX exceeds 64 MiB')
        pos = len(self.data)
        self.data.extend(b'\x00' * length)
        return pos

    def pack(self, fmt, pos, *values):
        struct.pack_into('<' + fmt, self.data, pos, *values)

    def rel(self, owner, field_pos, destination):
        delta = destination - owner
        if delta <= 0 or delta > 0x7fffffff:
            raise FormatError('invalid output offset')
        self.pack('i', owner + field_pos, delta)

    def copy(self, blob):
        pos = self.reserve(len(blob))
        self.data[pos:pos + len(blob)] = blob
        return pos


def convert_v7_to_v6(data, mdl=None):
    r = VtxReader(data)
    version, cache, max_strip, max_tri, max_vertex, checksum, lods, replacements, bodies, body_offset = r.unpack('iiHHiiiiii', 0)
    if version != 7:
        raise FormatError('input is not Source optimizedmodel VTX version 7')
    if not 1 <= lods <= 8 or not 1 <= bodies <= 256 or max_vertex > 3 or max_vertex < 1:
        raise FormatError('unsupported header counts or bone format')
    # V7 headers may advertise the *optimizer's* 512-bone limit even when
    # each actual strip fits the old 16-bone palette. Validate EVERY strip
    # individually, and advertise only the safe old caps in the output.
    if not 1 <= max_strip <= 512 or not 1 <= max_tri <= 12:
        raise FormatError('invalid or unsupported VTX bone limits')
    r.table(0, replacements, lods, 8, 8)
    r.table(0, body_offset, bodies, 8, 256)
    if mdl is not None:
        if len(mdl) < 80 or mdl[:4] != b'IDST' or struct.unpack_from('<I', mdl, 4)[0] not in (36, 37):
            raise FormatError('invalid or unsupported companion MDL')
        if struct.unpack_from('<I', mdl, 8)[0] != checksum:
            raise FormatError('MDL/VTX checksum mismatch')
        if struct.unpack_from('<I', mdl, 76)[0] != len(mdl):
            raise FormatError('MDL length mismatch')

    out = VtxBuilder()
    out.pack('iiHHiiiiii', 0, 6, cache, min(max_strip, 16), max_tri, 4, checksum, lods, 0, bodies, 0)

    old_repls = r.table(0, replacements, lods, 8, 8)
    new_repls = out.reserve(lods * 8)
    out.rel(0, 24, new_repls)
    for l in range(lods):
        at = old_repls + l * 8
        count, rel = r.unpack('ii', at)
        table = r.table(at, rel, count, 6, 2048)
        new_at = new_repls + l * 8
        out.pack('i', new_at, count)
        if count:
            target = out.reserve(count * 6)
            out.rel(new_at, 4, target)
            for k in range(count):
                old_item = table + 6 * k
                material, name_rel = r.unpack('hi', old_item)
                if material < 0:
                    raise FormatError('negative replacement material ID')
                new_item = target + 6 * k
                out.pack('h', new_item, material)
                out.rel(new_item, 2, out.copy(r.string(old_item, name_rel)))

    old_bodies = r.table(0, body_offset, bodies, 8, 256)
    new_bodies = out.reserve(bodies * 8)
    out.rel(0, 32, new_bodies)
    total_vertices = 0
    for b in range(bodies):
        old_b = old_bodies + b*8
        nmodels, models_rel = r.unpack('ii', old_b)
        old_models = r.table(old_b, models_rel, nmodels, 8, 1024)
        new_b = new_bodies + b*8
        out.pack('i', new_b, nmodels)
        new_models = out.reserve(nmodels*8)
        out.rel(new_b, 4, new_models)
        for m in range(nmodels):
            old_model = old_models + 8*m
            num_lods, lod_rel = r.unpack('ii', old_model)
            if num_lods < 1 or num_lods > lods:
                raise FormatError('model LOD count mismatch')
            old_lods = r.table(old_model, lod_rel, num_lods, 12, 8)
            new_model = new_models + 8*m
            out.pack('i', new_model, num_lods)
            new_lods = out.reserve(num_lods*12)
            out.rel(new_model, 4, new_lods)
            for l in range(num_lods):
                old_l = old_lods + 12*l
                nmeshes, mesh_rel, switch = r.unpack('iif', old_l)
                old_meshes = r.table(old_l, mesh_rel, nmeshes, 9, 8192)
                new_l = new_lods + 12*l
                out.pack('if', new_l, nmeshes, 0.0)  # replace switch at the actual offset below
                out.pack('f', new_l + 8, switch)
                if nmeshes == 0:
                    continue
                new_meshes = out.reserve(nmeshes*9)
                out.rel(new_l, 4, new_meshes)
                for k in range(nmeshes):
                    old_mesh = old_meshes + 9*k
                    groups, groups_rel, flags = r.unpack('iiB', old_mesh)
                    old_groups = r.table(old_mesh, groups_rel, groups, 25, 8192)
                    if flags & ~3:
                        raise FormatError('unsupported VTX mesh flags')
                    new_mesh = new_meshes + 9*k
                    out.pack('iB', new_mesh, groups, 0)
                    out.pack('B', new_mesh+8, flags)
                    if not groups:
                        continue
                    new_groups = out.reserve(groups*25)
                    out.rel(new_mesh, 4, new_groups)
                    for g in range(groups):
                        old_group = old_groups + 25*g
                        nv, vrel, ni, irel, ns, srel, group_flags = r.unpack('iiiiiiB',old_group)
                        if group_flags & ~3:
                            raise FormatError('v7 delta-flex strip group is not supported by v6')
                        old_verts = r.table(old_group, vrel, nv, 9, 65535)
                        old_indices = r.table(old_group, irel, ni, 2, 1_000_000)
                        old_strips = r.table(old_group, srel, ns, 27, 65535)
                        total_vertices += nv
                        if total_vertices > 1_000_000:
                            raise FormatError('unreasonable aggregate VTX vertex count')
                        new_group = new_groups + g*25
                        out.pack('iiiiiiB', new_group, nv, 0, ni, 0, ns, 0, group_flags)
                        if nv:
                            new_verts = out.reserve(15*nv)
                            out.rel(new_group,4,new_verts)
                            for v in range(nv):
                                vw0, vw1, vw2, n_bones, orig, b0, b1, b2 = r.unpack('BBBBhbbb',old_verts+9*v)
                                if n_bones > 3:
                                    raise FormatError('unsupported >3 v7 bone influences')
                                out.pack('4B4hhB',new_verts+15*v,vw0,vw1,vw2,0,b0,b1,b2,-1,orig,n_bones)
                        if ni:
                            indices = r.unpack('%dH'%ni,old_indices)
                            if any(i >= nv for i in indices):
                                raise FormatError('strip group contains an out-of-range vertex index')
                            out.rel(new_group,12,out.copy(data[old_indices:old_indices+ni*2]))
                        if ns:
                            new_strips = out.reserve(ns*27)
                            out.rel(new_group,20,new_strips)
                            for s in range(ns):
                                old_strip = old_strips + 27*s
                                nidx, idx_start, nvert, vert_start, bones, flg, changes, change_rel = r.unpack('iiiihBii',old_strip)
                                if (flg & ~3) or bones>16 or bones<0 or (flg & 3) not in (1,2):
                                    raise FormatError('invalid or unsupported v7 strip metadata')
                                if min(nidx,idx_start,nvert,vert_start,changes) < 0 or idx_start+nidx > ni or vert_start+nvert > nv:
                                    raise FormatError('out-of-bounds strip indices or vertices')
                                old_changes = r.table(old_strip,change_rel,changes,8,1024)
                                new_strip = new_strips+27*s
                                out.pack('iiiihBii',new_strip,nidx,idx_start,nvert,vert_start,bones,flg,changes,0)
                                if changes:
                                    out.rel(new_strip,23,out.copy(data[old_changes:old_changes+8*changes]))
    return bytes(out.data)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('vtx',type=pathlib.Path,help='original VTX v7 input')
    p.add_argument('--mdl',type=pathlib.Path,help='corresponding authorized MDL for checksum verification')
    p.add_argument('--output',type=pathlib.Path,required=True,help='new VTX v6 output, must not exist')
    args=p.parse_args()
    if args.output.exists():
        p.error('refusing to overwrite an existing file')
    converted=convert_v7_to_v6(args.vtx.read_bytes(),args.mdl.read_bytes() if args.mdl else None)
    args.output.open('xb').write(converted)
    print('Created %s (%d bytes); MDL/VVD and runtime compatibility not established' %(args.output,len(converted)))

if __name__=='__main__':
    main()
