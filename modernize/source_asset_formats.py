"""Defensive metadata reader for the *supplied* 32-bit studio.h model format.

Only studio versions 35/36 are decoded. Version 35 uses the same structures and
is explicitly converted by Studio_ConvertStudioHdrToNewVersion. A retail or later
beta MDL with a different version is not evidence of compatibility with this
engine. This is a metadata audit, not a geometry/animation/UV validator.
"""
from pathlib import Path
import struct

SUPPORTED_MDL_VERSIONS = (35, 36)
STUDIO_VERSION = 36
STUDIO_HEADER_BYTES = 372
MAX_MODEL_BYTES = 256 * 1024 * 1024


def inspect_mdl(path):
    """Return JSON-compatible metadata without loading executable game code."""
    result = dict(status='invalid', version=None, bones=0, bone_names=[],
                  sequence_names=[], activity_names=[], material_names=[],
                  material_directories=[], attachments=[], hitbox_sets=0,
                  sequence_groups=[], issues=[])
    try:
        path = Path(path)
        size = path.stat().st_size
        if size < 8 or size > MAX_MODEL_BYTES:
            raise ValueError('MDL size outside the bounded reader limit')
        with path.open('rb') as stream:
            prefix = stream.read(80)
            if prefix[:4] != b'IDST':
                raise ValueError('not an IDST studio model')
            version = struct.unpack_from('<i', prefix, 4)[0]
            result['version'] = version
            if version not in SUPPORTED_MDL_VERSIONS:
                result['status'] = 'unsupported_version'
                result['issues'].append(f'MDL version {version}; supplied engine STUDIO_VERSION is 36; metadata not decoded')
                return result
            if len(prefix) < 80:
                raise ValueError('truncated studio header')
            declared = struct.unpack_from('<i', prefix, 76)[0]
            if declared < STUDIO_HEADER_BYTES or declared > size:
                raise ValueError('declared MDL length exceeds file or truncates header')
            stream.seek(0)
            data = stream.read(declared)
        if declared != size:
            result['issues'].append('bytes follow the declared MDL length')

        def integer(offset):
            if offset < 0 or offset + 4 > len(data):
                raise ValueError('integer outside declared MDL length')
            return struct.unpack_from('<i', data, offset)[0]

        def string(offset):
            if offset < 0 or offset >= len(data):
                raise ValueError('string outside declared MDL length')
            end = data.find(b'\0', offset, min(len(data), offset + 4096))
            if end < 0:
                raise ValueError('unterminated or overlong MDL string')
            return data[offset:end].decode('ascii', errors='strict')

        def table(count_offset, index_offset, stride, name):
            count, index = integer(count_offset), integer(index_offset)
            if count < 0 or count > 65536 or (count and (index < STUDIO_HEADER_BYTES or index + count * stride > len(data))):
                raise ValueError(f'{name} table outside declared MDL length')
            return [index + i * stride for i in range(count)]

        # All offsets/strides are from public/studio.h, Windows 32-bit layout.
        bones = table(156, 160, 196, 'bone')
        result['bones'] = len(bones)
        result['bone_names'] = [string(base + integer(base)) for base in bones]
        hitboxes = table(172, 176, 12, 'hitbox set')
        result['hitbox_sets'] = len(hitboxes)
        for base in hitboxes:
            count, relative = integer(base + 4), integer(base + 8)
            if count < 0 or count > 65536 or (count and (base + relative < STUDIO_HEADER_BYTES or base + relative + count * 68 > len(data))):
                raise ValueError('hitbox table outside declared MDL length')
        # MAXSTUDIOBLENDS is 32 in this tree, unlike newer studio headers.
        sequences = table(188, 192, 4284, 'sequence')
        result['sequence_names'] = [string(base + integer(base)) for base in sequences]
        result['activity_names'] = [string(base + integer(base + 4)) if integer(base + 4) else '' for base in sequences]
        groups = table(200, 204, 48, 'sequence group')
        result['sequence_groups'] = [string(base + integer(base + 4)) if integer(base + 4) else '' for base in groups]
        textures = table(208, 212, 32, 'material')
        result['material_names'] = [string(base + integer(base)) for base in textures]
        dirs = table(216, 220, 4, 'material search directory')
        result['material_directories'] = [string(integer(base)) for base in dirs]
        attachment_bases = table(244, 248, 60, 'attachment')
        result['attachments'] = [string(base + integer(base)) for base in attachment_bases]
        for base in attachment_bases:
            bone = integer(base + 8)
            if bone < 0 or bone >= result['bones']:
                raise ValueError('attachment references an invalid bone')
        result['status'] = 'metadata_verified'
    except (OSError, ValueError, struct.error, UnicodeError) as exc:
        result['issues'].append(str(exc))
    return result
