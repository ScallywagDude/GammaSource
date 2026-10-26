#!/usr/bin/env python3
"""Replay-safe v21 legacy vertex check, with v36 behavior unchanged."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def change(path, old, new, marker):
    p=ROOT/path; raw=p.read_bytes(); crlf=b'\r\n' in raw
    s=raw.decode('latin1').replace('\r\n','\n')
    if marker in s: return
    if s.count(old)!=1: raise RuntimeError('%s: %s matches %d'%(path, marker,s.count(old)))
    s=s.replace(old,new,1)
    p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin1'))
change('engine/l_studio.cpp', '#include "../modernize/model_preview_studio_validation.h"', '''#include "../modernize/model_preview_studio_validation.h"
#include "../modernize/studio_sidecar_fs_v21.h"''', 'HL2MODERN_STUDIO_SIDECAR_V21')
change('engine/l_studio.cpp', '''        if (!compatible)
        {
            Warning("%s: v37 model requires a v36-compatible studio layout and legacy .vtx; rejected\\n", mod->name);''', '''        // HL2MODERN_STUDIO_SIDECAR_V21: check the actual v6 vertex header,
        // checksum and bodypart table, not just the existence of a .vtx.
        if (compatible)
        {
            static const char *const suffixes[]={".dx80.vtx",".dx7_3bone.vtx",".dx7_2bone.vtx"};
            const size_t len=strlen(mod->name);
            compatible=false;
            if (len>4 && len<220 && !Q_stricmp(mod->name+len-4,".mdl"))
            {
                char vtxPath[256];
                for (int i=0;i<3 && !compatible;++i)
                {
                    memcpy(vtxPath,mod->name,len-4);
                    strcpy(vtxPath+len-4,suffixes[i]);
                    compatible=HL2ModernStudioSidecar::LegacyVertexFile(
                        g_pFileSystem,vtxPath,(const unsigned char *)phdr,sizeof(studiohdr_t));
                }
            }
        }
        if (!compatible)
        {
            Warning("%s: v37 model requires a v36-compatible studio layout and legacy .vtx; rejected\\n", mod->name);''', 'HL2MODERN_STUDIO_SIDECAR_V21')
change('engine/gameui_modelpreview.cpp', '#include "../modernize/model_preview_studio_validation.h"', '''#include "../modernize/model_preview_studio_validation.h"
#include "../modernize/studio_sidecar_fs_v21.h"''', 'HL2MODERN_PREVIEW_SIDECAR_V21')
change('engine/gameui_modelpreview.cpp', '''            g_pFileSystem->FileExists(vertexFile)) return true;''', '''            g_pFileSystem->FileExists(vertexFile) &&
            (header.version!=37 || HL2ModernStudioSidecar::LegacyVertexFile(
                g_pFileSystem,vertexFile,(const unsigned char *)&header,sizeof(header)))) return true;
        // HL2MODERN_PREVIEW_SIDECAR_V21: the preview must reject retail v7
        // VTX/foreign checksums even if its filename looks valid.''', 'HL2MODERN_PREVIEW_SIDECAR_V21')
change('engine/gameui_modelpreview.cpp', '''            g_pFileSystem->FileExists(vertexFile);
    }

    virtual bool SetModel''', '''            g_pFileSystem->FileExists(vertexFile) &&
            (header.version!=37 || HL2ModernStudioSidecar::LegacyVertexFile(
                g_pFileSystem,vertexFile,(const unsigned char *)&header,sizeof(header)));
    }

    virtual bool SetModel''', 'HL2MODERN_PREVIEW_FALLBACK_SIDECAR_V21')
change('dlls/hl2_dll/hl2_player.cpp', '#include "../../modernize/model_preview_studio_validation.h"', '''#include "../../modernize/model_preview_studio_validation.h"
#include "../../modernize/studio_sidecar_fs_v21.h"''', 'HL2MODERN_PLAYER_SIDECAR_V21')
change('dlls/hl2_dll/hl2_player.cpp', '''&& filesystem->FileExists(vertexPath, "GAME"))
            return true;''', '''&& filesystem->FileExists(vertexPath, "GAME") &&
            (!v37 || HL2ModernStudioSidecar::LegacyVertexFile(
                filesystem,vertexPath,prefix,bytes)))
            return true; // HL2MODERN_PLAYER_SIDECAR_V21''', 'HL2MODERN_PLAYER_SIDECAR_V21')
change('dlls/player.cpp', '#include "../modernize/model_preview_studio_validation.h"', '''#include "../modernize/model_preview_studio_validation.h"
#include "../modernize/studio_sidecar_fs_v21.h"''', 'HL2MODERN_WEAPON_SIDECAR_V21')
change('dlls/player.cpp', '''            filesystem->FileExists(vertexPath,"GAME")) return true;''', '''            filesystem->FileExists(vertexPath,"GAME") &&
            (!v37 || HL2ModernStudioSidecar::LegacyVertexFile(
                filesystem,vertexPath,header,bytes))) return true; // HL2MODERN_WEAPON_SIDECAR_V21''', 'HL2MODERN_WEAPON_SIDECAR_V21')
print('v21 sidecar guards applied')
