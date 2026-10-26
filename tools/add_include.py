import sys
from pathlib import Path
p = Path(sys.argv[1]); after = ('#include "%s"' % sys.argv[2]).encode(); add = ('#include "%s"' % sys.argv[3]).encode()
d = p.read_bytes()
nl = b"\r\n" if b"\r\n" in d else b"\n"
assert d.count(after) == 1, after
if add not in d:
    d = d.replace(after, after + nl + add)
p.write_bytes(d)
print("ok")
