#!/usr/bin/env python3
"""v21 guarded v37 compatibility bridge; keep v36 behavior unchanged.

Models claiming version 37 must use the actual v36-compatible *data layout* and
legacy vertex companions. No version-number-only bypass is permitted.
"""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def patch(rel, replacements, marker):
    p=ROOT/rel
    raw=p.read_bytes(); crlf=b'\r\n' in raw
    s=raw.decode('latin1').replace('\r\n','\n')
    if marker in s: return
    for old,new in replacements:
        n=s.count(old)
        if n!=1: raise RuntimeError(f'{rel}: expected one occurrence, got {n} of {old[:100]!r}')
        s=s.replace(old,new,1)
    p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin1'))

patch('public/studio.h', [
('if( version >= 35 )', '''// HL2MODERN_STUDIO_V37_V21: legacy conversion never blindly rewrites newer
    // formats; v37 is admitted ONLY through the validated engine bridge.
    if( version >= 35 && version <= STUDIO_VERSION )''')
], 'HL2MODERN_STUDIO_V37_V21')

patch('modernize/model_preview_studio_validation.h', [
('''    bool Validate() const
    {''', '''    // HL2MODERN_STUDIO_LAYOUT_V37_V21: a v37-labelled blob is accepted ONLY
    // if all the pointer tables, sequence channels, meshes and strings can
    // be walked with this tree's v36 structure definitions. This does NOT
    // implement every distinct v37 layout found in later models.
    bool Validate(bool allowV37Layout = false) const
    {'''),
('''if (h->version != STUDIO_VERSION || h->length != m_Length ||''', '''if ((h->version != STUDIO_VERSION &&
             (!allowV37Layout || h->version != 37)) || h->length != m_Length ||''')
], 'HL2MODERN_STUDIO_LAYOUT_V37_V21')

patch('engine/l_studio.cpp', [
('#include "ModelInfo.h"', '#include "ModelInfo.h"\n#include "../modernize/model_preview_studio_validation.h"'),
('''\tStudio_ConvertStudioHdrToNewVersion( phdr );
\t
\tversion = LittleLong (phdr->version);''', '''    // HL2MODERN_STUDIO_LOADER_V37_V21: no blind 37->36 rewrite. For a
    // version-37 MDL, prove the on-disk bytes conform to our v36 table and
    // animation layouts *before* handing the buffer to legacy consumers.
    // Models with newer vertex formats or genuinely changed layouts fail
    // closed. This bridge is deliberately narrower than all retail v37.
    const int diskVersion = LittleLong(phdr->version);
    if (diskVersion == 37)
    {
        FileHandle_t disk = g_pFileSystem ? g_pFileSystem->Open(mod->name, "rb") : FILESYSTEM_INVALID_HANDLE;
        if (disk == FILESYSTEM_INVALID_HANDLE)
        {
            Warning("%s: v37 source file could not be opened\\n", mod->name);
            return false;
        }
        const int diskLength = g_pFileSystem->Size(disk);
        bool compatible = false;
        if (diskLength >= (int)sizeof(studiohdr_t) && diskLength <= 64*1024*1024 &&
            phdr->length == diskLength)
        {
            CUtlVector<byte> diskBytes;
            diskBytes.SetSize(diskLength);
            const int read = g_pFileSystem->Read(diskBytes.Base(), diskLength, disk);
            if (read == diskLength && !memcmp(buffer,diskBytes.Base(),diskLength))
                compatible = HL2ModernModelPreview::StudioDataValidator(
                    diskBytes.Base(),diskLength).Validate(true);
        }
        g_pFileSystem->Close(disk);
        if (!compatible)
        {
            Warning("%s: v37 model requires a v36-compatible studio layout and legacy .vtx; rejected\\n", mod->name);
            return false;
        }
        // Normalized only in the validated in-memory working copy; the disk
        // header remains v37 and normal v36 assets remain byte-identical.
        phdr->version = STUDIO_VERSION;
    }
    else
    {
        Studio_ConvertStudioHdrToNewVersion(phdr);
    }

\tversion = LittleLong (phdr->version);''')
], 'HL2MODERN_STUDIO_LOADER_V37_V21')

patch('engine/gameui_modelpreview.cpp', [
('''        // Do not load a retail/other-branch MDL into this engine just because
        // its filename matches. This renderer consumes the 2003 v36 format.''', '''        // HL2MODERN_PREVIEW_V37_V21: accept native v36 or v37 files proven
        // structurally compatible with the 2003 loader, not a version rename.'''),
('''header.version != STUDIO_VERSION || header.length < sizeof(header) ||''', '''(header.version != STUDIO_VERSION && header.version != 37) || header.length < sizeof(header) ||'''),
('''!HL2ModernModelPreview::StudioDataValidator(studioBytes.Base(),header.length).Validate()) return false;''', '''!HL2ModernModelPreview::StudioDataValidator(studioBytes.Base(),header.length).Validate(true)) return false;''')
], 'HL2MODERN_PREVIEW_V37_V21')

# Server and experimental weapon preflights validate the entire v37 MDL before
# offering its .dx80/.dx7 geometry to the actual engine loader.
patch('dlls/hl2_dll/hl2_player.cpp', [
('#include "studio.h"', '#include "studio.h"\n#include "../../modernize/model_preview_studio_validation.h"'),
('''    filesystem->Close(file);
    if (fileLength <= 0 || !HL2ModernPlayerAssets::StudioHeader(
        prefix, bytes, (unsigned long)fileLength, STUDIO_VERSION, sizeof(studiohdr_t))) return false;''', '''    // HL2MODERN_SERVER_V37_V21: validate the full v37 table layout, not
    // merely its IDST/version prefix, before exposing it to precaching.
    const bool v37 = bytes == (int)sizeof(prefix) &&
        HL2ModernPlayerAssets::ReadLE32(prefix+4) == 37;
    bool compatibleV37 = false;
    if (v37 && fileLength >= (int)sizeof(studiohdr_t) && fileLength <= 64*1024*1024 &&
        HL2ModernPlayerAssets::StudioHeader(prefix,bytes,(unsigned long)fileLength,37,sizeof(studiohdr_t)))
    {
        CUtlVector<byte> content;
        content.SetSize(fileLength);
        filesystem->Seek(file,0,FILESYSTEM_SEEK_HEAD);
        if (filesystem->Read(content.Base(),fileLength,file) == fileLength)
            compatibleV37 = HL2ModernModelPreview::StudioDataValidator(
                content.Base(),fileLength).Validate(true);
    }
    filesystem->Close(file);
    if (fileLength <= 0 || (!compatibleV37 && !HL2ModernPlayerAssets::StudioHeader(
        prefix, bytes, (unsigned long)fileLength, STUDIO_VERSION, sizeof(studiohdr_t)))) return false;''')
], 'HL2MODERN_SERVER_V37_V21')

patch('dlls/player.cpp', [
('#include "studio.h"', '#include "studio.h"\n#include "../modernize/model_preview_studio_validation.h"'),
('''    filesystem->Close(file);
    if (bytes!=(int)sizeof(header) || !HL2ModernWeapons::StudioHeader(
        header,(unsigned int)bytes,length,STUDIO_VERSION,sizeof(studiohdr_t))) return false;''', '''    // HL2MODERN_CHEAT_V37_V21: the experimental weapon must have a v36
    // compatible full studio layout even when labelled v37 on disk.
    const bool v37 = bytes == (int)sizeof(header) &&
        HL2ModernWeapons::Little32(header+4) == 37;
    bool compatibleV37 = false;
    if (v37 && length >= sizeof(studiohdr_t) && length <= 64*1024*1024 &&
        HL2ModernWeapons::StudioHeader(header,bytes,length,37,sizeof(studiohdr_t)))
    {
        CUtlVector<byte> content;
        content.SetSize((int)length);
        filesystem->Seek(file,0,FILESYSTEM_SEEK_HEAD);
        if (filesystem->Read(content.Base(),(int)length,file) == (int)length)
            compatibleV37 = HL2ModernModelPreview::StudioDataValidator(
                content.Base(),(int)length).Validate(true);
    }
    filesystem->Close(file);
    if (bytes!=(int)sizeof(header) || (!compatibleV37 && !HL2ModernWeapons::StudioHeader(
        header,(unsigned int)bytes,length,STUDIO_VERSION,sizeof(studiohdr_t)))) return false;''')
], 'HL2MODERN_CHEAT_V37_V21')
print('v21 guarded studio v37 bridge applied')
