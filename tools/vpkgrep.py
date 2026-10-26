"""List (and optionally extract) VPK entries whose path matches a regex.
usage: vpkgrep.py REGEX file.vpk [file.vpk ...] [--extract outdir]"""
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vpkshaders import entries, read_entry

args = sys.argv[1:]
out = None
if '--extract' in args:
    i = args.index('--extract'); out = args[i + 1]; del args[i:i + 2]
rx = re.compile(args[0], re.I)
for vpk in args[1:]:
    for name, arch, off, ln, pre, dataoff in entries(vpk):
        if not rx.search(name):
            continue
        print(f'{os.path.basename(vpk)}: {name} {ln + len(pre)}')
        if out:
            p = os.path.join(out, name)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, 'wb').write(read_entry(vpk, arch, off, ln, pre, dataoff))
