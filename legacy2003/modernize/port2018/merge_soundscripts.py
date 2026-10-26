#!/usr/bin/env python3
"""Build the 2018 build's sound scripts: GammaSource's 2003 scripts plus every retail HL2
entry they lack. GammaSource's own entries win where both define a sound.

The 2003 content folder overrides retail files by name, so without this the 2018 code
loses retail-only sounds (Weapon_PhysCannon.HoldSound, Weapon_StunStick.Activate, ...).

usage: merge_soundscripts.py <gammasource_content> <retail hl2 dir> <out content dir>
"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vpkshaders import entries, read_entry  # noqa: E402


def blocks(text):
    """Top-level  "name" { ... }  blocks of a KeyValues sound script, in order."""
    out, i, n = [], 0, len(text)
    while i < n:
        m = re.compile(r'"([^"]+)"\s*\{').search(text, i)
        if not m:
            break
        depth, j = 1, m.end()
        while j < n and depth:
            if text[j] == '{':
                depth += 1
            elif text[j] == '}':
                depth -= 1
            elif text[j] == '"':
                j = text.find('"', j + 1)
            elif text.startswith('//', j):
                j = text.find('\n', j)
                if j < 0:
                    j = n
            j += 1
        out.append((m.group(1), text[m.start():j]))
        i = j
    return out


def retail_file(retail, rel):
    p = os.path.join(retail, rel)
    if os.path.exists(p):
        return open(p, 'rb').read().decode('latin-1')
    for vpk in ('hl2_misc_dir.vpk', 'hl2_pak_dir.vpk'):
        vp = os.path.join(retail, vpk)
        if not os.path.exists(vp):
            continue
        for name, *rest in entries(vp):
            if name.lower() == rel.replace('\\', '/').lower():
                return read_entry(vp, *rest).decode('latin-1')
    return None


def manifest_files(text):
    return re.findall(r'"(?:pre(?:cache|load)_file)"\s*"([^"]+)"', text)


def main(gamma, retail, out):
    gman = open(os.path.join(gamma, 'scripts/game_sounds_manifest.txt'), 'rb').read().decode('latin-1')
    rman = retail_file(retail, 'scripts/game_sounds_manifest.txt') or ''
    gfiles, rfiles = manifest_files(gman), manifest_files(rman)
    os.makedirs(os.path.join(out, 'scripts'), exist_ok=True)

    added_total = 0
    for rel in gfiles:
        gpath = os.path.join(gamma, rel)
        rtext = retail_file(retail, rel)
        if not os.path.exists(gpath) or rtext is None:
            continue
        gtext = open(gpath, 'rb').read().decode('latin-1')
        have = {name.lower() for name, _ in blocks(gtext)}
        extra = [body for name, body in blocks(rtext) if name.lower() not in have]
        if not extra:
            continue
        merged = gtext.rstrip() + '\n\n// ---- GammaSource 2018 build: retail HL2 entries this file lacks ----\n\n' + '\n\n'.join(extra) + '\n'
        dst = os.path.join(out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, 'wb').write(merged.encode('latin-1'))
        added_total += len(extra)
        print('%-45s +%d retail entries' % (rel, len(extra)))

    # manifest: the 2003 list, then retail files it does not name
    extra_files = [f for f in rfiles if f.lower() not in {g.lower() for g in gfiles}]
    # sounds of the weapons added in the 2018 build (hand-written, lives in gammasource_content_2018)
    extra_files.append('scripts/game_sounds_gamma2018.txt')
    # the Portal-Base portal gun (Portal content is mounted after HL2)
    extra_files += ['scripts/game_sounds_portal.txt', 'scripts/game_sounds_player_portal.txt',
                    'scripts/game_sounds_weapons_portal.txt', 'scripts/npc_sounds_energy_ball.txt']
    lines = ''.join('\t"precache_file"\t\t"%s"\n' % f for f in extra_files)
    man = gman.rstrip()
    assert man.endswith('}')
    man = man[:-1] + '\t// GammaSource 2018 build: retail HL2 sound scripts\n' + lines + '}\n'
    open(os.path.join(out, 'scripts/game_sounds_manifest.txt'), 'wb').write(man.encode('latin-1'))
    print('manifest: +%d retail files; %d retail entries merged' % (len(extra_files), added_total))


if __name__ == '__main__':
    main(*sys.argv[1:4])
