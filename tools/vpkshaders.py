"""List shaders/fxc/*.vcs headers inside a set of VPKs (version, total combos, dyn combos, crc).
usage: vpkshaders.py name_substr dir.vpk [dir.vpk ...]   [--extract outdir]"""
import struct, sys, os

def read_cstr(f):
    b = bytearray()
    while True:
        c = f.read(1)
        if not c or c == b'\0':
            return b.decode('latin1')
        b += c

def entries(path):
    with open(path, 'rb') as f:
        sig, ver, tree = struct.unpack('<III', f.read(12))
        if ver == 2:
            f.read(16)
        hdr = f.tell()
        while True:
            ext = read_cstr(f)
            if not ext: break
            while True:
                d = read_cstr(f)
                if not d: break
                while True:
                    n = read_cstr(f)
                    if not n: break
                    crc, pre, arch, off, ln, term = struct.unpack('<IHHIIH', f.read(18))
                    predata = f.read(pre)
                    yield f'{d}/{n}.{ext}', arch, off, ln, predata, hdr + tree

def read_entry(path, arch, off, ln, predata, dataoff):
    base = path[:-len('_dir.vpk')]
    if arch == 0x7fff:
        with open(path, 'rb') as f:
            f.seek(dataoff + off); return predata + f.read(ln)
    with open(f'{base}_{arch:03d}.vpk', 'rb') as f:
        f.seek(off); return predata + f.read(ln)

if __name__ == '__main__':
    args = sys.argv[1:]
    out = None
    if '--extract' in args:
        i = args.index('--extract'); out = args[i + 1]; del args[i:i + 2]
    sub = args[0].lower()
    for vpk in args[1:]:
        for name, arch, off, ln, pre, dataoff in entries(vpk):
            if not name.startswith('shaders/') or sub not in name.lower():
                continue
            d = read_entry(vpk, arch, off, ln, pre, dataoff)
            v, total, dyn = struct.unpack_from('<iii', d, 0)
            print(f'{vpk.split(os.sep)[-3] if os.sep in vpk else vpk}: {name} ver={v} total={total} dyn={dyn} size={len(d)}')
            if out:
                p = os.path.join(out, name); os.makedirs(os.path.dirname(p), exist_ok=True)
                open(p, 'wb').write(d)
