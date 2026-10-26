"""Compare each fxctmp9 .inc's combo layout (static total x dynamic total) with a .vcs header.
usage: vcscheck.py <fxctmp9 dir> <shaders/fxc dir> [more fxc dirs, first wins]"""
import os, re, struct, sys

inc_dir, fxc_dirs = sys.argv[1], sys.argv[2:]

def inc_layout(path):
    t = open(path, encoding='latin-1').read()
    rets = re.findall(r'return\s*\((.*?)\);', t, re.S)
    if len(rets) < 2:
        return None
    def span(expr, base):
        terms = re.findall(r'\(\s*(\d+)\s*\*\s*m_n(\w+)\s*\)', expr)
        return terms
    st, dy = re.findall(r'\(\s*(\d+)\s*\*\s*m_n(\w+)\s*\)', rets[0]), re.findall(r'\(\s*(\d+)\s*\*\s*m_n(\w+)\s*\)', rets[1])
    # dynamic total = first static multiplier (or product inferred); total combos unknown upper bound
    ndyn = int(st[0][0]) if st else None
    return ndyn, st, dy

for name in sorted(os.listdir(inc_dir)):
    if not name.endswith('.inc'):
        continue
    base = name[:-4]
    vcs = None
    for d in fxc_dirs:
        p = os.path.join(d, base + '.vcs')
        if os.path.exists(p):
            vcs = p
            break
    if not vcs:
        continue
    lay = inc_layout(os.path.join(inc_dir, name))
    if not lay or lay[0] is None:
        continue
    ver, total, dyn = struct.unpack('<iii', open(vcs, 'rb').read(12))
    if dyn != lay[0]:
        print('%-45s inc dyn=%-4d vcs dyn=%-4d (%s)' % (base, lay[0], dyn, os.path.basename(os.path.dirname(os.path.dirname(vcs)))))
