"""Structural check of a converted v48 MDL (bones, animations, sequences).
usage: check_mdl48.py <converted.mdl>   (e.g. from <mod>/legacy_models_cache_v8)
Reports out-of-range indices, broken animation runs and non-finite values."""
import struct, sys, math
b = open(sys.argv[1], 'rb').read()
N = len(b)
I = lambda o: struct.unpack_from('<i', b, o)[0]
F = lambda o: struct.unpack_from('<f', b, o)[0]
H = lambda o: struct.unpack_from('<h', b, o)[0]
B = lambda o: b[o]
errs = []
def err(m):
    errs.append(m)
def cstr(o):
    if not (0 <= o < N): return None
    e = b.find(b'\0', o)
    return b[o:e].decode('latin1') if e >= 0 else None
assert b[:4] == b'IDST', 'not IDST'
ver = I(4)
print('version', ver, 'length', I(76), 'file', N)
if I(76) != N: err('length field %d != file size %d' % (I(76), N))
nb, bi = I(156), I(160)
nanim, ai = I(180), I(184)
nseq, si = I(188), I(192)
nik, iki = I(284), I(288)
print('bones', nb, 'anims', nanim, 'seqs', nseq, 'ikchains', nik)
# bones
phys = []
for k in range(nb):
    o = bi + 216 * k
    name = cstr(o + I(o))
    par = I(o + 4)
    if name is None: err('bone %d bad name' % k)
    if not (-1 <= par < k): err('bone %d parent %d' % (k, par))
    for f in range(32, 160, 4):
        if not math.isfinite(F(o + f)): err('bone %d non-finite field +%d' % (k, f)); break
    phys.append(I(o + 172))
print('physicsbone range', min(phys), max(phys))
# animations
STUDIO_ANIM_RAWPOS, STUDIO_ANIM_RAWROT, STUDIO_ANIM_ANIMPOS, STUDIO_ANIM_ANIMROT, STUDIO_ANIM_DELTA, STUDIO_ANIM_RAWROT2 = 1, 2, 4, 8, 16, 32
def walk_values(o, frames, what):
    # mstudioanimvalue_t run-length data covering 'frames' frames
    k = frames
    guard = 0
    while k > 0:
        if not (0 <= o and o + 2 <= N): err('%s value run out of file' % what); return
        valid, total = B(o), B(o + 1)
        if total == 0 or valid > total or valid == 0:
            err('%s bad run valid=%d total=%d' % (what, valid, total)); return
        if o + 2 * (valid + 1) > N: err('%s run data out of file' % what); return
        k -= total
        o += 2 * (valid + 1)
        guard += 1
        if guard > 100000: err('%s runaway' % what); return
for a in range(nanim):
    o = ai + 100 * a
    name = cstr(o + I(o + 4))
    frames = I(o + 16)
    fps = F(o + 8)
    if name is None: err('anim %d bad name' % a)
    if frames < 1 or frames > 100000: err('anim %d frames %d' % (a, frames))
    if not math.isfinite(fps) or fps <= 0: err('anim %d fps %r' % (a, fps))
    if I(o + 52) != 0: err('anim %d uses animblock %d' % (a, I(o + 52)))
    p = o + I(o + 56)
    seen = set()
    guard = 0
    while True:
        if not (0 <= p and p + 4 <= N): err('anim %d (%s) bone chain out of file' % (a, name)); break
        bone, flags, nxt = B(p), B(p + 1), H(p + 2)
        if bone >= nb: err('anim %d (%s) bone %d >= %d' % (a, name, bone, nb)); break
        if bone in seen: err('anim %d (%s) bone %d repeated' % (a, name, bone)); break
        seen.add(bone)
        q = p + 4
        if flags & STUDIO_ANIM_RAWROT: q += 6
        if flags & STUDIO_ANIM_RAWROT2: q += 8
        if flags & STUDIO_ANIM_ANIMROT:
            for c in range(3):
                off = H(q + 2 * c)
                if off: walk_values(q + off, frames, 'anim %d bone %d rot%d' % (a, bone, c))
            q += 6
        if flags & STUDIO_ANIM_RAWPOS: q += 6
        if flags & STUDIO_ANIM_ANIMPOS:
            for c in range(3):
                off = H(q + 2 * c)
                if off: walk_values(q + off, frames, 'anim %d bone %d pos%d' % (a, bone, c))
            q += 6
        if nxt == 0: break
        if nxt < 0: err('anim %d negative next' % a); break
        p += nxt
        guard += 1
        if guard > nb: err('anim %d chain too long' % a); break
    nr, ro = I(o + 60), I(o + 64)
    for r in range(nr):
        ro2 = o + ro + 152 * r
        if not (0 <= ro2 and ro2 + 152 <= N): err('anim %d ikrule %d out of file' % (a, r)); break
        chain, bone = I(ro2 + 8), I(ro2 + 12)
        if not (0 <= chain < max(1, nik)): err('anim %d ikrule %d chain %d' % (a, r, chain))
        if not (-1 <= bone < nb): err('anim %d ikrule %d bone %d' % (a, r, bone))
# sequences
acts = {}
for s_ in range(nseq):
    o = si + 212 * s_
    label = cstr(o + I(o + 4))
    act = cstr(o + I(o + 8))
    if label is None: err('seq %d bad label' % s_)
    gx, gy = I(o + 68), I(o + 72)
    if not (1 <= gx <= 32 and 1 <= gy <= 32): err('seq %d (%s) groupsize %d x %d' % (s_, label, gx, gy)); continue
    ii = o + I(o + 60)
    for k in range(gx * gy):
        v = H(ii + 2 * k)
        if not (0 <= v < nanim): err('seq %d (%s) blend %d -> anim %d' % (s_, label, k, v))
    ne, eo = I(o + 24), I(o + 28)
    for e in range(ne):
        eo2 = o + eo + 80 * e
        if not (0 <= eo2 and eo2 + 80 <= N): err('seq %d event %d out of file' % (s_, e)); break
        cyc = F(eo2)
        if not math.isfinite(cyc) or cyc < -0.01 or cyc > 1.01: err('seq %d (%s) event %d cycle %r' % (s_, label, e, cyc))
    wl = o + I(o + 156)
    for k in range(nb):
        w = F(wl + 4 * k)
        if not math.isfinite(w) or w < 0 or w > 1.001: err('seq %d (%s) weight bone %d = %r' % (s_, label, k, w)); break
    if act: acts[act] = acts.get(act, 0) + 1
print('activities', len(acts))
print('sample sequences:', ', '.join(cstr(si + 212 * k + I(si + 212 * k + 4)) for k in range(min(nseq, 12))))
print('ERRORS', len(errs))
for m in errs[:40]: print('  ', m)
