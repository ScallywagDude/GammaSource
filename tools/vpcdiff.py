"""List $File entries in vpc B that are not in vpc A (and A-only). usage: vpcdiff.py A.vpc B.vpc"""
import re, sys
def files(p):
    t = open(p, encoding='latin-1').read()
    return {m.lower().replace('/', '\\') for m in re.findall(r'\$File\s+"([^"]+)"', t)}
a, b = files(sys.argv[1]), files(sys.argv[2])
print('== only in', sys.argv[2])
for f in sorted(b - a): print(f)
print('== only in', sys.argv[1])
for f in sorted(a - b): print(f)
for m in re.findall(r'\$PreprocessorDefinitions\s+"([^"]+)"', open(sys.argv[2], encoding='latin-1').read()): print('defs B:', m)
