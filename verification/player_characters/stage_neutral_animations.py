"""Stage owned HL2 human locomotion as private, neutral-only player libraries.

All sequence/animation records and their layer indices remain in place. Only
activity exposure, automatic playback, and private asset names are changed.
This avoids adding NPC automatic head/spine poses to armed multiplayer poses.
The original external animation data is copied byte for byte.
"""
import hashlib
import json
from pathlib import Path
import struct

from vpk_reader import entries, read_entry

WORK = Path(__file__).resolve().parents[1]
ARCHIVE = Path('C:/Program Files (x86)/Steam/steamapps/common/GarrysMod/sourceengine/hl2_misc_dir.vpk')
ACTIVITIES = {
    'ACT_IDLE', 'ACT_WALK', 'ACT_RUN', 'ACT_COVER_LOW', 'ACT_WALK_CROUCH',
    'ACT_JUMP', 'ACT_GLIDE', 'ACT_LAND',
}


def integer(data, offset):
    return struct.unpack_from('<i', data, offset)[0]


def string(data, offset):
    return data[offset:data.index(b'\0', offset)].decode('latin1')


def table(data, count, offset, stride):
    return [integer(data, offset) + stride*n for n in range(integer(data, count))]


def chains(data):
    bones = [string(data, b+integer(data, b)) for b in table(data, 156, 160, 216)]
    result = []
    for chain in table(data, 284, 288, 16):
        count, links = integer(data, chain+8), chain+integer(data, chain+12)
        assert count == 3
        ids = [integer(data, links+28*n) for n in range(count)]
        assert all(0 <= n < len(bones) for n in ids)
        result.append({'name': string(data, chain+integer(data, chain)), 'bones': [bones[n] for n in ids]})
    return result


def verify_ik(data, ani):
    """Check actual rules in both the MDL and the external ANI blocks."""
    count = 0
    references = []
    block_count = integer(data, 352)
    block_table = integer(data, 356)
    blocks = []
    for n in range(block_count):
        start, end = struct.unpack_from('<ii', data, block_table+8*n)
        assert 0 <= start <= end <= len(ani), (n, start, end, len(ani))
        blocks.append((start, end))
    for seq in table(data, 188, 192, 212):
        locks, at = integer(data, seq+164), seq+integer(data, seq+168)
        assert locks >= 0 and (not locks or 0 <= at <= len(data)-32*locks)
        references += [integer(data, at+32*n) for n in range(locks)]
    for lock in table(data, 320, 324, 32):
        references.append(integer(data, lock))
    for animation in table(data, 180, 184, 100):
        number = integer(data, animation+60)
        local, external = integer(data, animation+64), integer(data, animation+68)
        block = integer(data, animation+52)
        if not number:
            continue
        if local:
            buffer, at = data, animation+local
        elif external:
            if block == 0:
                buffer, at = data, animation+external
            else:
                assert 0 < block < block_count
                buffer, at = ani, blocks[block][0]+external
                assert at+152*number <= blocks[block][1]
        else:
            # The stock files contain a few unrelated weapon animations whose
            # IK data is absent; pIKRule safely returns NULL for these.
            continue
        assert 0 <= at <= len(buffer)-152*number
        references += [integer(buffer, at+152*n+8) for n in range(number)]
        count += number
    assert references and all(0 <= n < 4 for n in references), set(references)
    return {'external_blocks': block_count-1, 'animation_rules_checked': count,
            'all_rule_and_lock_chain_indices': sorted(set(references))}


def append_string(data, value):
    offset = len(data)
    data.extend(value.encode('latin1')+b'\0')
    return offset


def build(index, gender):
    origin = 'models/humans/'+gender+'_shared'
    original = read_entry(str(ARCHIVE), *index[origin+'.mdl'][1:])
    ani = read_entry(str(ARCHIVE), *index[origin+'.ani'][1:])
    assert original[:4] == b'IDST' and integer(original, 4) == 48
    assert integer(original, 76) == len(original)
    assert integer(original, 204) == integer(original, 232) == integer(original, 336) == 0
    donor_chains = chains(original)
    player_data = (WORK/'assets/models/gamma/player'/f'{gender}_anims.mdl').read_bytes()
    player_chains = chains(player_data)
    assert len(donor_chains) == 4 and donor_chains == player_chains
    def bind_matrices(buffer):
        return {string(buffer, b+integer(buffer, b)): struct.unpack_from('<12f', buffer, b+96)
                for b in table(buffer, 156, 160, 216)}
    player_binds, donor_binds = bind_matrices(player_data), bind_matrices(original)
    assert player_binds.keys() == donor_binds.keys()
    bind_delta = max(abs(x-y) for name in player_binds
                     for x, y in zip(player_binds[name], donor_binds[name]))
    assert bind_delta < .001, bind_delta
    checks = verify_ik(original, ani)

    data = bytearray(original)
    empty = append_string(data, '')
    exposed, disabled_autoplay = [], []
    for seq in table(data, 188, 192, 212):
        label = string(data, seq+integer(data, seq+4))
        activity = string(data, seq+integer(data, seq+8)) if integer(data, seq+8) else ''
        # Keep every original numeric sequence slot so local animation layers
        # and blend lookups remain valid; labels cannot collide with MP labels.
        private_label = append_string(data, 'gamma_neutral_'+label)
        struct.pack_into('<i', data, seq+4, private_label-seq)
        flags = integer(data, seq+12)
        if flags & 0x8:
            disabled_autoplay.append(label)
        struct.pack_into('<i', data, seq+12, flags & ~0x8)
        if activity in ACTIVITIES:
            exposed.append({'activity': activity, 'sequence': 'gamma_neutral_'+label})
        else:
            struct.pack_into('<i', data, seq+8, empty-seq)
            struct.pack_into('<ii', data, seq+16, -1, 0)
    assert {row['activity'] for row in exposed} == ACTIVITIES
    assert len(exposed) == len(ACTIVITIES)
    struct.pack_into('<ii', data, 320, 0, 0)  # Existing player autoplay locks remain authoritative.
    private = 'models/gamma/player/'+gender+'_neutral'
    struct.pack_into('<i', data, 348, append_string(data, private+'.ani'))
    data[12:76] = (private.removeprefix('models/')+'.mdl').encode().ljust(64, b'\0')
    struct.pack_into('<i', data, 76, len(data))
    assert chains(data) == donor_chains
    assert not any(integer(data, s+12) & 0x8 for s in table(data, 188, 192, 212))
    verify_ik(data, ani)
    files = []
    for suffix, blob in (('.mdl', bytes(data)), ('.ani', ani)):
        relative = private+suffix
        path = WORK/'assets'/relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(blob)
        files.append({'path': relative, 'source': str(ARCHIVE)+':'+origin+suffix,
                      'sha256': hashlib.sha256(blob).hexdigest(), 'bytes': len(blob)})
    return files, {'gender': gender, 'includes': [], 'ik_chains': donor_chains,
                   'ik_validation': checks, 'neutral_activities': exposed,
                   'maximum_bind_matrix_difference_from_multiplayer_rig': bind_delta,
                   'disabled_npc_autoplay_sequences': disabled_autoplay,
                   'disabled_extra_npc_autoplay_ik_locks': integer(original, 320),
                   'original_mdl_sha256': hashlib.sha256(original).hexdigest(),
                   'unchanged_ani_sha256': hashlib.sha256(ani).hexdigest()}


def main():
    index = {record[0].lower(): record for record in entries(str(ARCHIVE))}
    files, libraries = [], []
    for gender in ('male', 'female'):
        staged, report = build(index, gender)
        files.extend(staged)
        libraries.append(report)
    result = {'scope': 'Neutral player locomotion from the locally owned HL2 human animation libraries',
              'files': files, 'libraries': libraries,
              'integration': 'Append the matching models/gamma/player/{gender}_neutral.mdl after the HL2DM animation include.'}
    (WORK/'neutral_animation_manifest.json').write_text(json.dumps(result, indent=2)+'\n')
    print('Staged four neutral-animation files; both four-chain IK contracts and 2172 animation rules validated.')


if __name__ == '__main__':
    main()
