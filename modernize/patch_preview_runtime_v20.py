#!/usr/bin/env python3
"""v20: fail-closed studio vertex path validation and preview matrix-state hygiene.

Replay on the v19 tree or a v20 tree; never alter unrelated project settings.
"""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]


def change(rel, replacements, marker):
    file = ROOT / rel
    raw = file.read_bytes()
    crlf = b'\r\n' in raw
    s = raw.decode('latin1').replace('\r\n', '\n')
    if marker in s:
        return
    for old, new in replacements:
        n = s.count(old)
        if n != 1:
            raise RuntimeError('%s: expected one occurrence of %r, found %d' % (rel, old[:100], n))
        s = s.replace(old, new, 1)
    file.write_bytes((s.replace('\n', '\r\n') if crlf else s).encode('latin1'))

change('modernize/hl2_player_asset_guard.h', [
('namespace HL2ModernPlayerAssets {',
'''#include <string.h>
#include <stddef.h>
namespace HL2ModernPlayerAssets {
// HL2MODERN_VERTEX_COMPANION_V20: derive only safe, bounded v36 vertex paths.
// The embedded studio name is relative to models/ and must end in .mdl.
inline bool VertexCompanionPath(const unsigned char *prefix, int bytes,
                               const char *suffix, char *out, size_t capacity)
{
    if (!prefix || bytes < 76 || !suffix || !out || !capacity) return false;
    out[0] = 0;
    static const char *const permitted[] = {
        ".dx80.vtx", ".dx7_3bone.vtx", ".dx7_2bone.vtx"
    };
    bool allowed = false;
    for (unsigned int s=0; s<3; ++s)
        if (!strcmp(suffix, permitted[s])) allowed = true;
    if (!allowed) return false;
    // Studio model name occupies exactly 64 bytes starting at offset 12.
    const unsigned char *name = prefix + 12;
    const unsigned char *ending = (const unsigned char *)memchr(name, 0, 64);
    if (!ending || ending == name) return false;
    const size_t length = (size_t)(ending-name);
    if (length < 5 || name[length-5] == '/' || name[length-4] != '.' ||
        (name[length-3]|32) != 'm' || (name[length-2]|32) != 'd' ||
        (name[length-1]|32) != 'l') return false;
    // The Source 2003 loader prepends models/ to this internal name.
    size_t segment = 0;
    for (size_t i=0; i<length; ++i)
    {
        const unsigned char ch = name[i];
        if (ch == '/' || i == length-1)
        {
            const size_t end = (ch=='/' ? i : i+1);
            const size_t span = end-segment;
            if (!span || (span==1 && name[segment]=='.') ||
                (span==2 && name[segment]=='.' && name[segment+1]=='.')) return false;
            segment=i+1;
        }
        if (ch=='\\\\' || ch==':' || ch < 32 || ch >= 127) return false;
    }
    const size_t suffixLen = strlen(suffix);
    const size_t total = 7 + length-4 + suffixLen;
    if (total >= capacity) return false;
    memcpy(out, "models/", 7);
    memcpy(out+7, name, length-4);
    memcpy(out+7+length-4, suffix, suffixLen+1);
    return true;
}
''')
], 'HL2MODERN_VERTEX_COMPANION_V20')

change('dlls/hl2_dll/hl2_player.cpp', [
('''    filesystem->Close(file);
    return fileLength > 0 && HL2ModernPlayerAssets::StudioHeader(
        prefix, bytes, (unsigned long)fileLength, STUDIO_VERSION, sizeof(studiohdr_t));
}''',
'''    filesystem->Close(file);
    if (fileLength <= 0 || !HL2ModernPlayerAssets::StudioHeader(
        prefix, bytes, (unsigned long)fileLength, STUDIO_VERSION, sizeof(studiohdr_t))) return false;
    // HL2MODERN_SERVER_VERTEX_PREFLIGHT_V20: keep the server's available-model
    // decision aligned with the 2003 vertex-file loader. This is still only a
    // preflight, not proof of valid meshes, rigging, or materials.
    static const char *const variants[] = {
        ".dx80.vtx", ".dx7_3bone.vtx", ".dx7_2bone.vtx"
    };
    char vertexPath[128];
    for (unsigned int i=0; i<3; ++i)
        if (HL2ModernPlayerAssets::VertexCompanionPath(prefix, bytes, variants[i],
              vertexPath, sizeof(vertexPath)) && filesystem->FileExists(vertexPath, "GAME"))
            return true;
    return false;
}''')
], 'HL2MODERN_SERVER_VERTEX_PREFLIGHT_V20')

change('engine/gameui_modelpreview.cpp', [
('#include "../modernize/model_preview_studio_validation.h"',
 '#include "../modernize/model_preview_studio_validation.h"\n#include "../modernize/hl2_player_asset_guard.h"'),
('''        // Match Mod_LoadStudioModelVtxFileIntoTempBuffer's actual 2003 names.
        // VVD and dx90.vtx are not the format consumed by this source tree.
        if (!header.name[0] || strstr(header.name, "..") || strchr(header.name, ':') ||
            header.name[0] == '/' || header.name[0] == '\\\\') return false;
        char vertexFile[128];
        Q_snprintf(vertexFile, sizeof(vertexFile), "models/%s", header.name);
        char *extension = strrchr(vertexFile, '.');
        if (!extension) return false;
        *extension = 0;
        const int stemLength = strlen(vertexFile);
        const char *suffix = ".dx7_2bone.vtx";
        if (g_pMaterialSystemHardwareConfig->SupportsVertexAndPixelShaders() ||
            g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 70)
            suffix = ".dx80.vtx";
        else if (g_pMaterialSystemHardwareConfig->MaxBlendMatrices() > 2)
            suffix = ".dx7_3bone.vtx";
        Q_strncpy(vertexFile + stemLength, suffix, sizeof(vertexFile) - stemLength);
        if (g_pFileSystem->FileExists(vertexFile)) return true;
        Q_strncpy(vertexFile + stemLength, ".dx7_2bone.vtx", sizeof(vertexFile) - stemLength);
        return g_pFileSystem->FileExists(vertexFile);''',
'''        // HL2MODERN_PREVIEW_VERTEX_PREFLIGHT_V20: use the same bounded
        // path generator as the HL2 server, with the loader's actual DX order.
        // VVD/dx90.vtx cannot substitute for 2003 dx80 or dx7 vertex data.
        const char *suffix = ".dx7_2bone.vtx";
        if (g_pMaterialSystemHardwareConfig->SupportsVertexAndPixelShaders() ||
            g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 70)
            suffix = ".dx80.vtx";
        else if (g_pMaterialSystemHardwareConfig->MaxBlendMatrices() > 2)
            suffix = ".dx7_3bone.vtx";
        char vertexFile[128];
        if (HL2ModernPlayerAssets::VertexCompanionPath((const unsigned char *)&header,
             sizeof(header), suffix, vertexFile, sizeof(vertexFile)) &&
            g_pFileSystem->FileExists(vertexFile)) return true;
        if (!strcmp(suffix, ".dx7_2bone.vtx")) return false;
        return HL2ModernPlayerAssets::VertexCompanionPath((const unsigned char *)&header,
            sizeof(header), ".dx7_2bone.vtx", vertexFile, sizeof(vertexFile)) &&
            g_pFileSystem->FileExists(vertexFile);'''),
('''        IMaterialSystem *materials = materialSystemInterface;
        const MaterialFogMode_t oldFog = materials->GetFogMode();
        materials->FogMode(MATERIAL_FOG_NONE);
        materials->MatrixMode(MATERIAL_PROJECTION);
        materials->LoadIdentity();''',
'''        IMaterialSystem *materials = materialSystemInterface;
        // HL2MODERN_PREVIEW_MATRIX_SCOPE_V20: 3D GameUI shares the engine's
        // matrices. Push ALL three before changing them, then restore even if
        // this function later gains an early return.
        class PreviewMatrixScope
        {
        public:
            explicit PreviewMatrixScope(IMaterialSystem *m)
                : material(m), fog(m->GetFogMode())
            {
                material->MatrixMode(MATERIAL_PROJECTION); material->PushMatrix();
                material->MatrixMode(MATERIAL_VIEW); material->PushMatrix();
                material->MatrixMode(MATERIAL_MODEL); material->PushMatrix();
                material->FogMode(MATERIAL_FOG_NONE);
            }
            ~PreviewMatrixScope()
            {
                material->MatrixMode(MATERIAL_MODEL); material->PopMatrix();
                material->MatrixMode(MATERIAL_VIEW); material->PopMatrix();
                material->MatrixMode(MATERIAL_PROJECTION); material->PopMatrix();
                material->MatrixMode(MATERIAL_MODEL);
                material->FogMode(fog);
            }
        private:
            IMaterialSystem *material;
            MaterialFogMode_t fog;
        } scope(materials);
        materials->MatrixMode(MATERIAL_PROJECTION);
        materials->LoadIdentity();'''),
('''        materials->SetNumBoneWeights(0);
        materials->FogMode(oldFog);
        if (sequence < 0)''',
'''        materials->SetNumBoneWeights(0);
        if (sequence < 0)''')
], 'HL2MODERN_PREVIEW_MATRIX_SCOPE_V20')

change('dlls/player.cpp', [
('#include "../modernize/weapon_grant_policy.h"',
 '#include "../modernize/weapon_grant_policy.h"\n#include "../modernize/hl2_player_asset_guard.h"'),
('''    filesystem->Close(file);
    return bytes==(int)sizeof(header) && HL2ModernWeapons::StudioHeader(header,(unsigned int)bytes,length,STUDIO_VERSION,sizeof(studiohdr_t));
}''',
'''    filesystem->Close(file);
    if (bytes!=(int)sizeof(header) || !HL2ModernWeapons::StudioHeader(
        header,(unsigned int)bytes,length,STUDIO_VERSION,sizeof(studiohdr_t))) return false;
    // HL2MODERN_CHEAT_VERTEX_PREFLIGHT_V20: do not grant invisible prototype
    // weapons solely because a matching v35/v36 MDL header is installed.
    // The 2003 model loader needs a supported vertex companion for geometry.
    static const char *const variants[] = {
        ".dx80.vtx", ".dx7_3bone.vtx", ".dx7_2bone.vtx"
    };
    char vertexPath[128];
    for (unsigned int v=0; v<3; ++v)
        if (HL2ModernPlayerAssets::VertexCompanionPath(header, bytes,
              variants[v],vertexPath,sizeof(vertexPath)) &&
            filesystem->FileExists(vertexPath,"GAME")) return true;
    return false;
}''')
], 'HL2MODERN_CHEAT_VERTEX_PREFLIGHT_V20')

print('v20 preview runtime and companion validation patches applied')
