"""Compare struct/lump definitions in the 2003 and 2018 bspfile.h (and friends)."""
import re, sys, difflib
from pathlib import Path
OLD = Path(r"C:\Users\doge6\Videos\GammaSource-main\public")
NEW = Path(r"C:\Users\doge6\Downloads\source-engine-master\source-engine-master\public")

def structs(text):
    text = re.sub(r'//[^\n]*', '', text)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    out = {}
    for m in re.finditer(r'struct\s+(\w+)\s*(?::[^{]*)?\{', text):
        depth, i = 1, m.end()
        while depth and i < len(text):
            depth += text[i] == '{'; depth -= text[i] == '}'; i += 1
        body = text[m.end():i - 1]
        # drop inline functions
        body = re.sub(r'\b\w[\w\s\*&:<>,]*\([^)]*\)\s*(const)?\s*\{[^{}]*\}', '', body)
        fields = [re.sub(r'\s+', ' ', f).strip() for f in body.split(';')]
        fields = [f for f in fields if f and not f.startswith(('DECLARE_', 'public', 'private', 'inline', 'friend'))]
        out[m.group(1)] = fields
    return out

for name in sys.argv[1:]:
    o = structs((OLD / name).read_text(errors='replace'))
    n = structs((NEW / name).read_text(errors='replace'))
    print('#####', name)
    for k in sorted(set(o) & set(n)):
        if o[k] != n[k]:
            print('== changed', k)
            for line in difflib.unified_diff(o[k], n[k], lineterm='', n=0):
                if line.startswith(('---', '+++', '@@')): continue
                print('   ', line)
    print('only 2003:', sorted(set(o) - set(n)))
    print('only 2018:', sorted(set(n) - set(o)))
    ol = re.findall(r'(LUMP_\w+)\s*=\s*(\d+)', (OLD / name).read_text(errors='replace'))
    nl = re.findall(r'(LUMP_\w+)\s*=\s*(\d+)', (NEW / name).read_text(errors='replace'))
    od, nd = dict((b, a) for a, b in ol), dict((b, a) for a, b in nl)
    for idx in sorted(set(od) | set(nd), key=int):
        if od.get(idx) != nd.get(idx):
            print('   lump', idx, od.get(idx), '->', nd.get(idx))
