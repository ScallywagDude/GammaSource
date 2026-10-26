"""Extract files whose VPK path matches a regex. usage: vpkextract.py <regex> <outdir> <dir.vpk>..."""
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vpkshaders import entries, read_entry  # noqa: E402  (same VPK reader)

pat = re.compile(sys.argv[1], re.I)
out = sys.argv[2]
for vpk in sys.argv[3:]:
    for name, arch, off, ln, pre, dataoff in entries(vpk):
        if not pat.search(name):
            continue
        d = read_entry(vpk, arch, off, ln, pre, dataoff)
        p = os.path.join(out, name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'wb').write(d)
        print(name, len(d))
