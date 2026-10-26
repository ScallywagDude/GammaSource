#!/usr/bin/env python3
"""Convert 2001-2003 studio models (MDL v36, or v37 via model37) to the 2018
engine's MDL v48 + .vvd + .dx90/.dx80/.sw VTX7 set.

Everything is rebuilt from decoded data rather than byte-patched:
  * bones, controllers, hitboxes, attachments, pose parameters, textures,
    skins, body parts, models and meshes are re-laid out in v48 structures;
  * vertices (inline in v36) are written to a .vvd file; weights beyond three
    bones are dropped and renormalised (v48 limit);
  * every animation is decoded frame by frame with the 2003 rules
    (game_shared/bone_setup.cpp) and re-encoded with v48 rules: constant bones
    as raw Quaternion64/Vector48, moving bones as RLE Euler/position channels
    with per-bone scales;
  * sequences keep their labels, activities, events, blend grids, bone weight
    lists, fades, node transitions and movement;
  * the VTX6 strip data is rewritten as VTX7 (9-byte vertices).
Not carried over yet: flexes/eyeballs (facial animation), IK rules/locks,
procedural bones and autolayers; they are reported per model.
"""
import argparse, math, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import model37

# ---------------------------------------------------------------- helpers
def i32(b, o): return struct.unpack_from('<i', b, o)[0]
def u16(b, o): return struct.unpack_from('<H', b, o)[0]
def f32(b, o): return struct.unpack_from('<f', b, o)[0]
def fv(b, o, n): return list(struct.unpack_from('<%df' % n, b, o))
def cstr(b, o):
    if o <= 0 or o >= len(b): return ''
    e = b.find(b'\0', o)
    return b[o:e].decode('latin-1')
def rstr(b, base, rel):
    return cstr(b, base + rel) if rel else ''


class ConvertError(ValueError):
    pass

# ------------------------------------------------------------- math (2003/2018 rules)
def angle_quaternion(a):          # RadianEuler (x=roll, y=pitch, z=yaw) -> Quaternion (x,y,z,w)
    sr, cr = math.sin(a[0] * 0.5), math.cos(a[0] * 0.5)
    sp, cp = math.sin(a[1] * 0.5), math.cos(a[1] * 0.5)
    sy, cy = math.sin(a[2] * 0.5), math.cos(a[2] * 0.5)
    srXcp, crXsp = sr * cp, cr * sp
    crXcp, srXsp = cr * cp, sr * sp
    return (srXcp * cy - crXsp * sy, crXsp * cy + srXcp * sy, crXcp * sy - srXsp * cy, crXcp * cy + srXsp * sy)

def quaternion_angles(q):         # inverse of the above (via matrix, as mathlib does)
    x, y, z, w = q
    m00 = 1.0 - 2.0 * y * y - 2.0 * z * z
    m10 = 2.0 * x * y + 2.0 * w * z
    m20 = 2.0 * x * z - 2.0 * w * y
    m21 = 2.0 * y * z + 2.0 * w * x
    m22 = 1.0 - 2.0 * x * x - 2.0 * y * y
    m01 = 2.0 * x * y - 2.0 * w * z
    m11 = 1.0 - 2.0 * x * x - 2.0 * z * z
    xy = math.sqrt(m00 * m00 + m10 * m10)
    if xy > 0.001:
        yaw = math.atan2(m10, m00); pitch = math.atan2(-m20, xy); roll = math.atan2(m21, m22)
    else:
        yaw = math.atan2(-m01, m11); pitch = math.atan2(-m20, xy); roll = 0.0
    return [roll, pitch, yaw]

def qnorm(q):
    l = math.sqrt(sum(c * c for c in q)) or 1.0
    return tuple(c / l for c in q)

# ----------------------------------------------------------- v36 animation decode
def animvalue_at(b, ptr, frame):
    """2003 RLE channel: value at an integer frame (no interpolation needed per frame)."""
    k = frame
    valid, total = b[ptr], b[ptr + 1]
    if total < valid: k = 0
    guard = 0
    while total <= k:
        k -= total
        ptr += (valid + 1) * 2
        valid, total = b[ptr], b[ptr + 1]
        if total < valid: k = 0
        guard += 1
        if guard > 100000: raise ConvertError('runaway animvalue stream')
    if valid > k:
        return struct.unpack_from('<h', b, ptr + (k + 1) * 2)[0]
    return struct.unpack_from('<h', b, ptr + valid * 2)[0]

def decode_v36_anim(b, ad, bones, numframes):
    """Return per bone: list of (pos, quat) per frame."""
    animbase = ad + i32(b, ad + 48)
    out = []
    for bi, bone in enumerate(bones):
        a = animbase + 32 * bi
        flags = i32(b, a)
        frames = []
        for fr in range(numframes):
            if flags & 2:   # STUDIO_ROT_ANIMATED
                ang = []
                for j in range(3):
                    off = i32(b, a + 4 + 4 * (j + 3))
                    if off == 0: ang.append(bone['value'][j + 3])
                    else: ang.append(bone['value'][j + 3] + animvalue_at(b, a + off, fr) * bone['scale'][j + 3])
                q = angle_quaternion(ang)
            else:
                q = qnorm(fv(b, a + 4 + 12, 4))
            if flags & 1:   # STUDIO_POS_ANIMATED
                p = []
                for j in range(3):
                    off = i32(b, a + 4 + 4 * j)
                    p.append(bone['value'][j] + (animvalue_at(b, a + off, fr) * bone['scale'][j] if off else 0.0))
            else:
                p = fv(b, a + 4, 3)
            frames.append((p, q))
        out.append(frames)
    return out

# ----------------------------------------------------------- v48 animation encode
def rle_channel(values):
    """Encode shorts as 2018 RLE spans (valid values, total frames). Simple, valid encoding."""
    out = bytearray()
    i, n = 0, len(values)
    while i < n:
        # run of repeats -> one valid value covering 'total' frames
        j = i
        while j + 1 < n and values[j + 1] == values[i] and j - i < 254: j += 1
        run = j - i + 1
        if run >= 3:
            out += struct.pack('<BB', 1, run) + struct.pack('<h', values[i])
            i = j + 1
            continue
        # literal block
        j = i
        lit = []
        while j < n and len(lit) < 255:
            k = j
            while k + 1 < n and values[k + 1] == values[j] and k - j < 254: k += 1
            if k - j + 1 >= 3: break
            lit.append(values[j]); j += 1
        out += struct.pack('<BB', len(lit), len(lit)) + struct.pack('<%dh' % len(lit), *lit)
        i = j
    return bytes(out)

def quat64(q):
    x, y, z, w = qnorm(q)
    def enc(v): return max(0, min(2097151, int(round(v * 1048576.5 + 1048576.5))))
    return struct.pack('<Q', enc(x) | (enc(y) << 21) | (enc(z) << 42) | ((1 if w < 0 else 0) << 63))

def vec48(p):
    return struct.pack('<3e', *[max(-65504.0, min(65504.0, c)) for c in p])

# ---------------------------------------------------------------- converter
class Writer:
    def __init__(self): self.b = bytearray(); self.strings = []
    def align(self, n=4):
        while len(self.b) % n: self.b.append(0)
    def alloc(self, size, align=4):
        self.align(align); o = len(self.b); self.b += bytes(size); return o
    def put(self, o, fmt, *v): struct.pack_into('<' + fmt, self.b, o, *v)
    def string(self, owner, field, text):
        """Queue a string; field (absolute) gets offset relative to owner."""
        self.strings.append((owner, field, text))
    def flush_strings(self):
        pool = {}
        for owner, field, text in self.strings:
            if text not in pool:
                pool[text] = len(self.b)
                self.b += text.encode('latin-1') + b'\0'
            struct.pack_into('<i', self.b, field, pool[text] - owner)


def convert(mdl, vtx6_by_suffix, name_hint=''):
    report = []
    if i32(mdl, 4) == 37:
        mdl, _ = model37.convert(mdl)
    if mdl[:4] != b'IDST' or i32(mdl, 4) != 36:
        raise ConvertError('not an MDL v36/v37 model')
    H = lambda o: i32(mdl, o)
    checksum = H(8)

    # --- bones
    nb, bi = H(156), H(160)
    bones = []
    for k in range(nb):
        o = bi + 196 * k
        bones.append({'name': rstr(mdl, o, i32(mdl, o)), 'parent': i32(mdl, o + 4),
                      'ctrl': list(struct.unpack_from('<6i', mdl, o + 8)),
                      'value': fv(mdl, o + 32, 6), 'scale': fv(mdl, o + 56, 6),
                      'poseToBone': fv(mdl, o + 80, 12), 'qAlign': fv(mdl, o + 128, 4),
                      'flags': i32(mdl, o + 144), 'proctype': i32(mdl, o + 148),
                      'surfaceprop': rstr(mdl, o, i32(mdl, o + 160)), 'quat': fv(mdl, o + 164, 4),
                      'contents': i32(mdl, o + 180)})
        if bones[-1]['proctype']: report.append('procedural bone %s dropped' % bones[-1]['name'])

    # --- animations (decoded)
    na, ai = H(180), H(184)
    anims = []
    for k in range(na):
        o = ai + 92 * k
        nf = max(1, i32(mdl, o + 12))
        anims.append({'name': rstr(mdl, o, i32(mdl, o)), 'fps': f32(mdl, o + 4), 'flags': i32(mdl, o + 8),
                      'numframes': nf, 'movements': [mdl[o + i32(mdl, o + 20) + 44 * m: o + i32(mdl, o + 20) + 44 * (m + 1)]
                                                    for m in range(i32(mdl, o + 16))],
                      'frames': decode_v36_anim(mdl, o, bones, nf)})
        if i32(mdl, o + 52): report.append('anim %s: %d IK rules dropped' % (anims[-1]['name'], i32(mdl, o + 52)))

    # per-bone scales for the new encoding (shared by every animation of that bone)
    posscale = [[0.0] * 3 for _ in bones]; rotscale = [[0.0] * 3 for _ in bones]
    base_rot = [b_['value'][3:6] for b_ in bones]; base_pos = [b_['value'][0:3] for b_ in bones]
    euler_cache = {}
    for ai_, an in enumerate(anims):
        delta = bool(an['flags'] & 0x0004)
        for bi_, frames in enumerate(an['frames']):
            eul = []
            prev = None
            for p, q in frames:
                e = quaternion_angles(q)
                if prev:  # keep Euler channels continuous
                    for j in range(3):
                        while e[j] - prev[j] > math.pi: e[j] -= 2 * math.pi
                        while e[j] - prev[j] < -math.pi: e[j] += 2 * math.pi
                eul.append(e); prev = e
                for j in range(3):
                    posscale[bi_][j] = max(posscale[bi_][j], abs(p[j] - (0.0 if delta else base_pos[bi_][j])) / 32767.0)
            for e in eul:
                for j in range(3):
                    rotscale[bi_][j] = max(rotscale[bi_][j], abs(e[j] - (0.0 if delta else base_rot[bi_][j])) / 32767.0)
            euler_cache[(ai_, bi_)] = eul

    # --- sequences
    ns, si = H(188), H(192)
    seqs = []
    for k in range(ns):
        o = si + 4284 * k
        gx, gy = max(1, i32(mdl, o + 4156)), max(1, i32(mdl, o + 4160))
        blends = [i32(mdl, o + 56 + 4 * (x * 32 + y)) for y in range(gy) for x in range(gx)]
        events = []
        for e in range(i32(mdl, o + 20)):
            eo = o + i32(mdl, o + 24) + 76 * e
            events.append((f32(mdl, eo), i32(mdl, eo + 4), i32(mdl, eo + 8), mdl[eo + 12:eo + 76]))
        wl = i32(mdl, o + 4248)
        weights = list(struct.unpack_from('<%df' % nb, mdl, o + wl)) if wl else [1.0] * nb
        pk = i32(mdl, o + 4252)
        if i32(mdl, o + 4240): report.append('seq %s: autolayers dropped' % rstr(mdl, o, i32(mdl, o)))
        if i32(mdl, o + 4236) or i32(mdl, o + 4256): report.append('seq %s: IK data dropped' % rstr(mdl, o, i32(mdl, o)))
        seqs.append({'label': rstr(mdl, o, i32(mdl, o)), 'activity': rstr(mdl, o, i32(mdl, o + 4)),
                     'flags': i32(mdl, o + 8), 'actweight': i32(mdl, o + 16), 'events': events,
                     'bb': fv(mdl, o + 28, 6), 'numblends': i32(mdl, o + 52), 'blends': blends, 'gx': gx, 'gy': gy,
                     'paramindex': list(struct.unpack_from('<2i', mdl, o + 4164)),
                     'paramstart': fv(mdl, o + 4172, 2), 'paramend': fv(mdl, o + 4180, 2),
                     'paramparent': i32(mdl, o + 4188), 'fadein': f32(mdl, o + 4196), 'fadeout': f32(mdl, o + 4200),
                     'entry': i32(mdl, o + 4204), 'exit': i32(mdl, o + 4208), 'nodeflags': i32(mdl, o + 4212),
                     'entryphase': f32(mdl, o + 4216), 'exitphase': f32(mdl, o + 4220), 'lastframe': f32(mdl, o + 4224),
                     'nextseq': i32(mdl, o + 4228), 'pose': i32(mdl, o + 4232), 'weights': weights,
                     'posekeys': mdl[o + pk:o + pk + 4 * (gx + gy)] if pk else None})

    # --- misc tables
    def table(cnt, idx, size): return [H(idx) + size * k for k in range(H(cnt))]
    ctrls = [mdl[o:o + 56] for o in table(164, 168, 56)]
    hbsets = []
    for o in table(172, 176, 12):
        boxes = [(mdl[ho:ho + 68], rstr(mdl, ho, i32(mdl, ho + 40))) for ho in
                 [o + i32(mdl, o + 8) + 68 * k for k in range(i32(mdl, o + 4))]]
        hbsets.append((rstr(mdl, o, i32(mdl, o)), boxes))
    textures = [(rstr(mdl, o, i32(mdl, o)), i32(mdl, o + 4)) for o in table(208, 212, 32)]
    cdtex = [cstr(mdl, i32(mdl, H(220) + 4 * k)) for k in range(H(216))]
    nskinref, nfam, skinidx = H(224), H(228), H(232)
    skins = mdl[skinidx: skinidx + 2 * nskinref * nfam]
    atts = [(rstr(mdl, o, i32(mdl, o)), i32(mdl, o + 4), i32(mdl, o + 8), mdl[o + 12:o + 60]) for o in table(244, 248, 60)]
    ntrans, transidx = H(252), H(256)
    transitions = mdl[transidx:transidx + ntrans * ntrans] if ntrans else b''
    poses = [(rstr(mdl, o, i32(mdl, o)), mdl[o + 4:o + 20]) for o in table(300, 304, 20)]
    ikchains = []
    for o in table(284, 288, 16):
        links = mdl[o + i32(mdl, o + 12): o + i32(mdl, o + 12) + 28 * i32(mdl, o + 8)]
        ikchains.append((rstr(mdl, o, i32(mdl, o)), i32(mdl, o + 4), i32(mdl, o + 8), links))
    if H(260) or H(268): report.append('flex data (%d descs, %d controllers) not carried over yet' % (H(260), H(268)))
    surfaceprop = cstr(mdl, H(308))
    keyvalues = mdl[H(312):H(312) + H(316)] if H(316) else b''

    # This archival helper has no full mesh-flex/eyeball serialization. Do not
    # silently manufacture a facial model with missing iris/eyelid references.
    # The production runtime converter preserves these fields in cache v4.
    if H(260) or H(268) or H(276):
        raise ConvertError('Facial flex data is unsupported by this offline helper; use the production runtime legacy model converter (cache v4).')
    for part in table(236, 240, 16):
        for model in [part + i32(mdl, part + 12) + 140 * k for k in range(i32(mdl, part + 4))]:
            if i32(mdl, model + 100):
                raise ConvertError('Eyeball/eyelid data is unsupported by this offline helper; use the production runtime legacy model converter (cache v4).')

    # --- bodyparts / models / meshes / vertices
    bodyparts = []
    vvd_verts = bytearray(); vvd_tan = bytearray()
    vert_used_bones = set()
    total_verts = 0
    for o in table(236, 240, 16):
        models = []
        for mo in [o + i32(mdl, o + 12) + 140 * k for k in range(i32(mdl, o + 4))]:
            nv = i32(mdl, mo + 80)
            vbase, tbase = mo + i32(mdl, mo + 84), mo + i32(mdl, mo + 88)
            start = total_verts
            for v in range(nv):
                vo = vbase + 64 * v
                w = fv(mdl, vo, 4); bn = list(struct.unpack_from('<4h', mdl, vo + 16)); cnt = struct.unpack_from('<h', mdl, vo + 24)[0]
                pairs = sorted([(w[j], bn[j]) for j in range(max(0, min(4, cnt)))], reverse=True)[:3]
                s = sum(p[0] for p in pairs) or 1.0
                pairs = [(p[0] / s, p[1]) for p in pairs]
                while len(pairs) < 3: pairs.append((0.0, 0))
                for p in pairs[:max(1, min(3, cnt))]: vert_used_bones.add(p[1])
                vvd_verts += struct.pack('<3f3bB', *[p[0] for p in pairs], *[p[1] for p in pairs], max(1, min(3, cnt)))
                vvd_verts += mdl[vo + 32:vo + 64]       # pos, normal, uv
                vvd_tan += mdl[tbase + 16 * v: tbase + 16 * v + 16] if i32(mdl, mo + 88) else struct.pack('<4f', 1, 0, 0, 1)
            total_verts += nv
            meshes = []
            for me in [mo + i32(mdl, mo + 76) + 68 * k for k in range(i32(mdl, mo + 72))]:
                if i32(mdl, me + 16): report.append('mesh flexes dropped')
                meshes.append({'material': i32(mdl, me), 'numvertices': i32(mdl, me + 8), 'vertexoffset': i32(mdl, me + 12),
                               'materialtype': i32(mdl, me + 24), 'materialparam': i32(mdl, me + 28), 'meshid': i32(mdl, me + 32),
                               'center': fv(mdl, me + 36, 3)})
            if i32(mdl, mo + 100): report.append('model %s: eyeballs dropped' % mdl[mo:mo + 64].split(b'\0')[0].decode('latin-1'))
            models.append({'name': mdl[mo:mo + 64], 'type': i32(mdl, mo + 64), 'radius': f32(mdl, mo + 68),
                           'numvertices': nv, 'start': start, 'meshes': meshes})
        bodyparts.append((rstr(mdl, o, i32(mdl, o)), i32(mdl, o + 8), models))

    # --- bone usage flags (v48 needs them to set up bones at all)
    BONE_USED_BY_HITBOX, BONE_USED_BY_ATTACHMENT, BONE_USED_BY_VERTEX_LOD0 = 0x100, 0x200, 0x400
    LODMASK = 0x400 | 0x800 | 0x1000 | 0x2000 | 0x4000 | 0x8000 | 0x10000 | 0x20000
    flags = [0] * nb
    def up(bi_, f):
        while 0 <= bi_ < nb:
            flags[bi_] |= f; bi_ = bones[bi_]['parent']
    for bi_ in vert_used_bones: up(bi_, LODMASK)
    for _, boxes in hbsets:
        for raw, _n in boxes: up(i32(raw, 0), BONE_USED_BY_HITBOX)
    for _n, _t, bone, _m in atts: up(bone, BONE_USED_BY_ATTACHMENT)

    # ================================================================ write v48
    w = Writer()
    hdr = w.alloc(408)
    w.b[0:4] = b'IDST'; w.put(4, 'i', 48); w.put(8, 'i', checksum)
    w.b[12:76] = mdl[12:76]
    w.b[80:156] = mdl[80:156]               # eye/illum/hull/view bb/flags (same offsets)
    # bones
    bo = w.alloc(216 * nb); w.put(156, 'ii', nb, bo)
    for k, bn_ in enumerate(bones):
        o = bo + 216 * k
        w.string(o, o, bn_['name'])
        w.put(o + 4, 'i', bn_['parent']); w.put(o + 8, '6i', *bn_['ctrl'])
        w.put(o + 32, '3f', *bn_['value'][0:3]); w.put(o + 44, '4f', *bn_['quat']); w.put(o + 60, '3f', *bn_['value'][3:6])
        w.put(o + 72, '3f', *posscale[k]); w.put(o + 84, '3f', *rotscale[k])
        w.put(o + 96, '12f', *bn_['poseToBone']); w.put(o + 144, '4f', *bn_['qAlign'])
        w.put(o + 160, 'i', flags[k]); w.put(o + 172, 'i', -1)
        w.string(o, o + 176, bn_['surfaceprop'] or surfaceprop or 'default'); w.put(o + 180, 'i', bn_['contents'])
    # bone table by name
    btn = w.alloc(nb, 1); w.put(364, 'i', btn)
    for pos_, k in enumerate(sorted(range(nb), key=lambda k: bones[k]['name'].lower())):
        w.b[btn + pos_] = k
    # controllers
    co = w.alloc(56 * len(ctrls)); w.put(164, 'ii', len(ctrls), co)
    for k, raw in enumerate(ctrls): w.b[co + 56 * k: co + 56 * (k + 1)] = raw
    # hitbox sets
    hs = w.alloc(12 * len(hbsets)); w.put(172, 'ii', len(hbsets), hs)
    for k, (name, boxes) in enumerate(hbsets):
        o = hs + 12 * k
        w.string(o, o, name)
        bx = w.alloc(68 * len(boxes)); w.put(o + 4, 'ii', len(boxes), bx - o)
        for j, (raw, bname) in enumerate(boxes):
            bo_ = bx + 68 * j
            w.b[bo_:bo_ + 40] = raw[0:40]
            if bname: w.string(bo_, bo_ + 40, bname)
    # animations
    ad = w.alloc(100 * len(anims)); w.put(180, 'ii', len(anims), ad)
    for k, an in enumerate(anims):
        o = ad + 100 * k
        w.put(o, 'i', -o); w.string(o, o + 4, an['name'])
        w.put(o + 8, 'fii', an['fps'], an['flags'], an['numframes'])
        if an['movements']:
            mv = w.alloc(44 * len(an['movements']))
            for j, raw in enumerate(an['movements']): w.b[mv + 44 * j: mv + 44 * (j + 1)] = raw
            w.put(o + 20, 'ii', len(an['movements']), mv - o)
        delta = 0x10 if an['flags'] & 0x0004 else 0
        start = None; prev = None
        for bi_ in range(nb):
            frames = an['frames'][bi_]
            ps = [f[0] for f in frames]; qs = [f[1] for f in frames]
            const = all(max(abs(a - b) for a, b in zip(p, ps[0])) < 1e-4 for p in ps) and \
                all(max(abs(a - b) for a, b in zip(q, qs[0])) < 1e-6 for q in qs)
            w.align(2)
            ao = len(w.b)
            if const:
                w.b += struct.pack('<BBh', bi_, 0x20 | 0x01 | delta, 0)     # RAWROT2 | RAWPOS
                w.b += quat64(qs[0]) + vec48(ps[0])
            else:
                eul = euler_cache[(k, bi_)]
                w.b += struct.pack('<BBh', bi_, 0x08 | 0x04 | delta, 0)     # ANIMROT | ANIMPOS
                vp_rot = len(w.b); w.b += bytes(6)
                vp_pos = len(w.b); w.b += bytes(6)
                for j in range(3):
                    sc = rotscale[bi_][j]
                    if sc > 0:
                        vals = [int(round((e[j] - (0.0 if delta else base_rot[bi_][j])) / sc)) for e in eul]
                        if any(vals):
                            struct.pack_into('<h', w.b, vp_rot + 2 * j, len(w.b) - vp_rot)
                            w.b += rle_channel(vals)
                for j in range(3):
                    sc = posscale[bi_][j]
                    if sc > 0:
                        vals = [int(round((p[j] - (0.0 if delta else base_pos[bi_][j])) / sc)) for p in ps]
                        if any(vals):
                            struct.pack_into('<h', w.b, vp_pos + 2 * j, len(w.b) - vp_pos)
                            w.b += rle_channel(vals)
            if start is None: start = ao
            if prev is not None: struct.pack_into('<h', w.b, prev + 2, ao - prev)
            prev = ao
        w.put(o + 52, 'ii', 0, (start - o) if start is not None else 0)
    # sequences
    sq = w.alloc(212 * len(seqs)); w.put(188, 'ii', len(seqs), sq)
    for k, s in enumerate(seqs):
        o = sq + 212 * k
        w.put(o, 'i', -o); w.string(o, o + 4, s['label']); w.string(o, o + 8, s['activity'])
        w.put(o + 12, 'iii', s['flags'], 0, s['actweight'])
        if s['events']:
            ev = w.alloc(80 * len(s['events'])); w.put(o + 24, 'ii', len(s['events']), ev - o)
            for j, (cyc, event, typ, opts) in enumerate(s['events']):
                eo = ev + 80 * j
                w.put(eo, 'fii', cyc, event, typ); w.b[eo + 12:eo + 76] = opts
        w.put(o + 32, '6f', *s['bb'])
        w.put(o + 56, 'i', max(1, s['numblends']))
        bl = w.alloc(2 * len(s['blends']), 2)
        for j, a in enumerate(s['blends']): struct.pack_into('<h', w.b, bl + 2 * j, a)
        w.put(o + 60, 'i', bl - o)
        w.put(o + 68, '2i', s['gx'], s['gy']); w.put(o + 76, '2i', *s['paramindex'])
        w.put(o + 84, '2f', *s['paramstart']); w.put(o + 92, '2f', *s['paramend']); w.put(o + 100, 'i', s['paramparent'])
        w.put(o + 104, '2f', s['fadein'] or 0.2, s['fadeout'] or 0.2)
        w.put(o + 112, '3i', s['entry'], s['exit'], s['nodeflags'])
        w.put(o + 124, '3f', s['entryphase'], s['exitphase'], s['lastframe'])
        w.put(o + 136, '2i', s['nextseq'], s['pose'])
        wl = w.alloc(4 * nb); w.put(wl, '%df' % nb, *s['weights']); w.put(o + 156, 'i', wl - o)
        if s['posekeys']:
            pk = w.alloc(len(s['posekeys'])); w.b[pk:pk + len(s['posekeys'])] = s['posekeys']; w.put(o + 160, 'i', pk - o)
    # textures, cd paths, skins
    tx = w.alloc(64 * len(textures)); w.put(204, 'ii', len(textures), tx)
    for k, (name, fl) in enumerate(textures):
        o = tx + 64 * k; w.string(o, o, name); w.put(o + 4, 'i', fl)
    cd = w.alloc(4 * len(cdtex)); w.put(212, 'ii', len(cdtex), cd)
    for k, p in enumerate(cdtex): w.string(0, cd + 4 * k, p)
    sk = w.alloc(len(skins), 2); w.b[sk:sk + len(skins)] = skins; w.put(220, 'iii', nskinref, nfam, sk)
    # bodyparts / models / meshes
    bp = w.alloc(16 * len(bodyparts)); w.put(232, 'ii', len(bodyparts), bp)
    lod_vertex_total = total_verts
    next_meshid = [0]
    for k, (name, base, models) in enumerate(bodyparts):
        o = bp + 16 * k
        w.string(o, o, name)
        mo = w.alloc(148 * len(models)); w.put(o + 4, 'iii', len(models), base, mo - o)
        for j, m in enumerate(models):
            mo_ = mo + 148 * j
            w.b[mo_:mo_ + 64] = m['name']
            w.put(mo_ + 64, 'if', m['type'], m['radius'])
            me = w.alloc(116 * len(m['meshes']))
            w.put(mo_ + 72, 'ii', len(m['meshes']), me - mo_)
            w.put(mo_ + 80, 'iii', m['numvertices'], 48 * m['start'], 16 * m['start'])
            for q, msh in enumerate(m['meshes']):
                mm = me + 116 * q
                w.put(mm, 'iiii', msh['material'], mo_ - mm, msh['numvertices'], msh['vertexoffset'])
                w.put(mm + 24, 'iii', msh['materialtype'], msh['materialparam'], next_meshid[0]); next_meshid[0] += 1  # v48 needs a unique ordinal
                w.put(mm + 36, '3f', *msh['center'])
                w.put(mm + 52, '8i', msh['numvertices'], *([msh['numvertices']] * 7))
    # attachments
    at = w.alloc(92 * len(atts)); w.put(240, 'ii', len(atts), at)
    for k, (name, typ, bone, local) in enumerate(atts):
        o = at + 92 * k
        w.string(o, o, name); w.put(o + 4, 'ii', typ, bone); w.b[o + 12:o + 60] = local
    # node transitions
    if ntrans:
        nn = w.alloc(4 * ntrans)
        for k in range(ntrans): w.string(0, nn + 4 * k, '')
        tr = w.alloc(len(transitions), 1); w.b[tr:tr + len(transitions)] = transitions
        w.put(248, 'iii', ntrans, tr, nn)
    # IK chains
    if ikchains:
        ik = w.alloc(16 * len(ikchains)); w.put(284, 'ii', len(ikchains), ik)
        for k, (name, typ, nl, links) in enumerate(ikchains):
            o = ik + 16 * k
            w.string(o, o, name)
            lk = w.alloc(len(links)); w.b[lk:lk + len(links)] = links
            w.put(o + 4, 'iii', typ, nl, lk - o)
    # pose parameters
    pp = w.alloc(20 * len(poses)); w.put(300, 'ii', len(poses), pp)
    for k, (name, rest) in enumerate(poses):
        o = pp + 20 * k; w.string(o, o, name); w.b[o + 4:o + 20] = rest
    # surface prop, key values, mass/contents
    w.string(0, 308, surfaceprop or 'default')
    if keyvalues:
        kv = w.alloc(len(keyvalues) + 1, 1); w.b[kv:kv + len(keyvalues)] = keyvalues; w.put(312, 'ii', kv, len(keyvalues))
    w.b[328:336] = mdl[328:336]
    w.b[377] = 0; w.b[378] = 0
    w.flush_strings()
    w.align(4)
    w.put(76, 'i', len(w.b))
    mdl48 = bytes(w.b)

    # ------------------------------------------------------------ VVD
    hdr_vvd = struct.pack('<4sii i8i iiii', b'IDSV', 4, checksum, 1, lod_vertex_total, *([lod_vertex_total] * 7),
                          0, 0, 64, 64 + len(vvd_verts))
    vvd = hdr_vvd + bytes(vvd_verts) + bytes(vvd_tan)

    # ------------------------------------------------------------ VTX6 -> VTX7
    vtx_out = {}
    for suffix, data in vtx6_by_suffix.items():
        try:
            vtx_out[suffix] = vtx6_to_7(data, checksum)
        except (ConvertError, struct.error) as e:
            report.append('%s not converted: %s' % (suffix, e))
    if '.dx90.vtx' not in vtx_out and '.dx80.vtx' in vtx_out:
        vtx_out['.dx90.vtx'] = vtx_out['.dx80.vtx']   # v48 looks for dx90 first
    if not vtx_out:
        raise ConvertError('no usable VTX file')
    return mdl48, vvd, vtx_out, report


def vtx6_to_7(d, checksum):
    """Walk a VTX6 file and rebuild it as VTX7 (9-byte vertices, max 3 bones)."""
    if i32(d, 0) != 6: raise ConvertError('VTX is not version 6')
    # header: version, vertCacheSize, maxBonesPerStrip(H), maxBonesPerFace(H), maxBonesPerVert, checkSum, numLODs, matRepListOffset, numBodyParts, bodyPartOffset
    version, vcache, mbs, mbf, mbv, crc, nlod, repl, nbp, bpo = struct.unpack_from('<iiHHiiiiii', d, 0)
    out = bytearray(struct.pack('<iiHHiiiiii', 7, vcache, mbs, mbf, min(3, mbv), checksum, nlod, 0, nbp, 36))
    # body parts / models / lods / meshes / strip groups, breadth first with fixups
    bp_new = len(out); out += bytes(8 * nbp)
    for b in range(nbp):
        ob = bpo + 8 * b
        nm, mo = i32(d, ob), ob + i32(d, ob + 4)
        m_new = len(out); out += bytes(8 * nm)
        struct.pack_into('<ii', out, bp_new + 8 * b, nm, m_new - (bp_new + 8 * b))
        for m in range(nm):
            om = mo + 8 * m
            nl, lo = i32(d, om), om + i32(d, om + 4)
            l_new = len(out); out += bytes(12 * nl)
            struct.pack_into('<ii', out, m_new + 8 * m, nl, l_new - (m_new + 8 * m))
            for l in range(nl):
                ol = lo + 12 * l
                nme, meo, switch = i32(d, ol), ol + i32(d, ol + 4), f32(d, ol + 8)
                me_new = len(out); out += bytes(9 * nme)
                struct.pack_into('<iif', out, l_new + 12 * l, nme, me_new - (l_new + 12 * l), switch)
                for me in range(nme):
                    ome = meo + 9 * me
                    ng, go, mflags = i32(d, ome), ome + i32(d, ome + 4), d[ome + 8]
                    g_new = len(out); out += bytes(25 * ng)
                    struct.pack_into('<iiB', out, me_new + 9 * me, ng, g_new - (me_new + 9 * me), mflags)
                    for g in range(ng):
                        og = go + 25 * g
                        nv, vo, ni, io, nst, so, gflags = struct.unpack_from('<iiiiiiB', d, og)
                        gn = g_new + 25 * g
                        # vertices
                        v_new = len(out)
                        for v in range(nv):
                            bw = d[og + vo + 15 * v: og + vo + 15 * v + 4]
                            bids = struct.unpack_from('<4h', d, og + vo + 15 * v + 4)
                            orig, nbones = struct.unpack_from('<HB', d, og + vo + 15 * v + 12)
                            nbones = min(3, nbones)
                            out += struct.pack('<3BBH3b', bw[0], bw[1], bw[2], nbones, orig,
                                               *[max(-128, min(127, x)) for x in bids[:3]])
                        i_new = len(out); out += d[og + io: og + io + 2 * ni]
                        while len(out) % 1: out.append(0)
                        s_new = len(out); out += bytes(27 * nst)
                        for s in range(nst):
                            os_ = og + so + 27 * s
                            nidx, idxo, nvert, verto, nb_, sflags, nchg, chgo = struct.unpack_from('<iiiihBii', d, os_)
                            c_new = len(out); out += d[os_ + chgo: os_ + chgo + 8 * nchg]
                            struct.pack_into('<iiiihBii', out, s_new + 27 * s, nidx, idxo, nvert, verto, nb_, sflags, nchg,
                                             c_new - (s_new + 27 * s))
                        struct.pack_into('<iiiiiiB', out, gn, nv, v_new - gn, ni, i_new - gn, nst, s_new - gn, gflags)
    # material replacement lists
    mr_new = len(out); out += bytes(8 * nlod)
    struct.pack_into('<i', out, 24, mr_new)    # materialReplacementListOffset
    for l in range(nlod):
        ol = repl + 8 * l
        n, ro = i32(d, ol), ol + i32(d, ol + 4)
        r_new = len(out); out += bytes(6 * n)
        struct.pack_into('<ii', out, mr_new + 8 * l, n, r_new - (mr_new + 8 * l))
        for r in range(n):
            orr = ro + 6 * r
            mid, so_ = struct.unpack_from('<hi', d, orr)
            s_new = len(out); out += cstr(d, orr + so_).encode('latin-1') + b'\0'
            struct.pack_into('<hi', out, r_new + 6 * r, mid, s_new - (r_new + 6 * r))
    return bytes(out)


def convert_file(mdl_path, out_dir, rel):
    stem = mdl_path.with_suffix('')
    vtx = {}
    for suf in ('.dx80.vtx', '.dx90.vtx', '.dx7_2bone.vtx', '.sw.vtx'):
        p = Path(str(stem) + suf)
        if p.exists():
            try:
                if i32(p.read_bytes(), 0) == 6: vtx[suf] = p.read_bytes()
            except Exception: pass
    mdl48, vvd, vtxs, report = convert(mdl_path.read_bytes(), vtx, rel)
    dest = out_dir / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.with_suffix('.mdl').write_bytes(mdl48)
    Path(str(dest.with_suffix('')) + '.vvd').write_bytes(vvd)
    for suf, data in vtxs.items():
        Path(str(dest.with_suffix('')) + suf).write_bytes(data)
    phy = Path(str(stem) + '.phy')
    if phy.exists():
        Path(str(dest.with_suffix('')) + '.phy').write_bytes(phy.read_bytes())
    return report


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('root', type=Path, help='game folder containing models/')
    ap.add_argument('--out', type=Path, required=True, help='output game folder')
    ap.add_argument('--only', default='', help='substring filter on model paths')
    a = ap.parse_args(argv)
    ok = bad = 0
    for mdl in sorted((a.root / 'models').rglob('*.mdl')):
        rel = mdl.relative_to(a.root).as_posix().lower()
        if a.only and a.only.lower() not in rel: continue
        try:
            head = mdl.read_bytes()[:8]
            if head[:4] != b'IDST' or struct.unpack_from('<i', head, 4)[0] not in (36, 37):
                continue
            rep = convert_file(mdl, a.out, Path(rel))
            ok += 1
            if rep: print(rel, '|', '; '.join(sorted(set(rep)))[:300])
        except (ConvertError, model37.FormatError, struct.error, ValueError, IndexError) as e:
            bad += 1
            print('FAIL', rel, '-', e)
    print('converted', ok, 'failed', bad)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
