#!/usr/bin/env python3
"""Reapply GammaSource weapon repairs after regenerating the 2018 port.

Idempotent, exact-context patches: a conflicting source edit stops preflight
before ANY files are written. --check only verifies applicability. The default
roots refer to this machine; both roots can be overridden for an extracted port.
"""
from pathlib import Path
import argparse, datetime, json, shutil

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine-root', type=Path, default=Path(r'C:\Users\doge6\Downloads\source-engine-master\source-engine-master'))
    parser.add_argument('--repo-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    roots = {'engine': args.engine_root.resolve(), 'repo': args.repo_root.resolve()}
    entries = json.loads(Path(__file__).with_name('weapon_repairs.json').read_text())
    pending, errors = [], []
    for entry in entries:
        root = roots[entry['root']]
        path = (root / entry['path']).resolve()
        if not path.is_relative_to(root):
            errors.append('Path escaped root: ' + str(path))
            continue
        old = path.read_text() if path.exists() else None
        text = old
        if 'create' in entry:
            if old is None:
                text = entry['create']
            elif old != entry['create']:
                errors.append('Existing generated file differs: ' + str(path))
                continue
        else:
            if text is None:
                errors.append('Missing source: ' + str(path))
                continue
            for edit in entry['edits']:
                if edit['new'] in text:
                    continue
                if text.count(edit['old']) != 1:
                    errors.append('Context differs: ' + str(path))
                    break
                text = text.replace(edit['old'], edit['new'], 1)
        if text != old:
            pending.append((entry, path, old, text))
    if errors:
        raise SystemExit('No files changed.\n' + '\n'.join(errors))
    if args.check:
        print(f'Applicable: {len(pending)} files need repair; {len(entries)-len(pending)} already repaired.')
        return
    backup = args.repo_root / 'modernize/port2018/weapon_repair_backups' / datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    for entry, path, old, text in pending:
        if old is not None:
            target = backup / entry['root'] / entry['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        print('Repaired ' + str(path))
    print(f'Complete: {len(pending)} files repaired; {len(entries)-len(pending)} already repaired.')

if __name__ == '__main__':
    main()
