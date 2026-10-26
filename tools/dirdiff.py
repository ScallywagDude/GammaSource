"""Per-file changed-line counts between two directories (same relative names).
usage: dirdiff.py A B [--show name]"""
import difflib, os, sys
a, b = sys.argv[1], sys.argv[2]
show = sys.argv[sys.argv.index('--show') + 1].lower() if '--show' in sys.argv else None
def read(p):
    return open(p, encoding='latin-1').read().replace('\r\n', '\n').splitlines()
names = sorted(set(os.listdir(a)) | set(os.listdir(b)), key=str.lower)
lower_a = {n.lower(): n for n in os.listdir(a)}
lower_b = {n.lower(): n for n in os.listdir(b)}
for key in sorted(set(lower_a) | set(lower_b)):
    na, nb = lower_a.get(key), lower_b.get(key)
    if not na or not nb:
        print('%-45s only in %s' % (key, 'A' if na else 'B'))
        continue
    la, lb = read(os.path.join(a, na)), read(os.path.join(b, nb))
    d = [l for l in difflib.unified_diff(la, lb, lineterm='', n=0) if l[:1] in '+-' and not l.startswith(('+++', '---'))]
    if d:
        print('%-45s %d changed lines' % (key, len(d)))
    if show and key == show:
        print('\n'.join(difflib.unified_diff(la, lb, 'A/' + key, 'B/' + key, lineterm='', n=2)))
