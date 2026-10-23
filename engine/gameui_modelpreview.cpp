// HL2MODERN_ANIMATED_PREVIEW: actual 2003 studio model rendering for GameUI.
// This file is excluded from the dedicated server configurations.
#include "quakedef.h"
#include "glquake.h"
#include "gl_model_private.h"
#include "modelgen.h"
#include "gl_matsysiface.h"
#include "filesystem_engine.h"
#include "filesystem.h"
#include "modelloader.h"
#include "istudiorender.h"
#include "studio.h"
#include "studio_internal.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "../game_shared/bone_setup.h"
#include "IGameUIModelPreview.h"
#include "../modernize/hl2_playermodel_policy.h"
#include "../modernize/model_preview_policy.h"
#include "../modernize/model_preview_studio_validation.h"
#include "../modernize/studio_sidecar_fs_v21.h"
#include "../modernize/hl2_player_asset_guard.h"
#include "tier0/memdbgon.h"

class CGameUIModelPreview : public IGameUIModelPreview
{
public:
    CGameUIModelPreview() { m_ModelPath[0] = 0; m_Status = "Select an installed model"; }

    virtual bool IsModelAvailable( const char *path )
    {
        // HL2MODERN_PREVIEW_V37_V21: accept native v36 or v37 files proven
        // structurally compatible with the 2003 loader, not a version rename.
        if (!HL2ModernPlayerModels::Find(path) || !g_pFileSystem ||
            !g_pMaterialSystemHardwareConfig) return false;
        FileHandle_t file = g_pFileSystem->Open(path, "rb");
        if (!file) return false;
        studiohdr_t header;
        const int size = g_pFileSystem->Size(file);
        const int read = g_pFileSystem->Read(&header, sizeof(header), file);
        if (read != sizeof(header) || header.id != IDSTUDIOHEADER ||
            (header.version != STUDIO_VERSION && header.version != 37) || header.length < sizeof(header) ||
            header.length > size || header.length > 64*1024*1024 || !memchr(header.name, 0, sizeof(header.name)) ||
            header.numbones <= 0 || header.numbones > MAXSTUDIOBONES ||
            header.numseq <= 0 || header.numbodyparts <= 0 ||
            header.numposeparameters < 0 || header.numposeparameters > MAXSTUDIOPOSEPARAM)
        {
            g_pFileSystem->Close(file);
            return false;
        }
        CUtlVector<byte> studioBytes;
        studioBytes.SetSize(header.length);
        g_pFileSystem->Seek(file,0,FILESYSTEM_SEEK_HEAD);
        const int bytesRead = g_pFileSystem->Read(studioBytes.Base(),header.length,file);
        g_pFileSystem->Close(file);
        if (bytesRead != header.length ||
            !HL2ModernModelPreview::StudioDataValidator(studioBytes.Base(),header.length).Validate(true)) return false;

        // HL2MODERN_PREVIEW_VERTEX_PREFLIGHT_V20: use the same bounded
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
            g_pFileSystem->FileExists(vertexFile) &&
            (header.version!=37 || HL2ModernStudioSidecar::LegacyVertexFile(
                g_pFileSystem,vertexFile,(const unsigned char *)&header,sizeof(header)))) return true;
        // HL2MODERN_PREVIEW_SIDECAR_V21: the preview must reject retail v7
        // VTX/foreign checksums even if its filename looks valid.
        if (!strcmp(suffix, ".dx7_2bone.vtx")) return false;
        return HL2ModernPlayerAssets::VertexCompanionPath((const unsigned char *)&header,
            sizeof(header), ".dx7_2bone.vtx", vertexFile, sizeof(vertexFile)) &&
            g_pFileSystem->FileExists(vertexFile) &&
            (header.version!=37 || HL2ModernStudioSidecar::LegacyVertexFile(
                g_pFileSystem,vertexFile,(const unsigned char *)&header,sizeof(header)));
    }

    virtual bool SetModel( const char *path )
    {
        if (path && !strcmp(path, m_ModelPath)) return true;
        ClearModel();
        if (!IsModelAvailable(path))
        {
            m_Status = "Compatible MDL or vertex data unavailable";
            return false;
        }
        Q_strncpy(m_ModelPath, path, sizeof(m_ModelPath));
        m_Status = "Animated 3D preview - drag to rotate";
        return true;
    }

    virtual void ClearModel()
    {
        // A distinct reference bit never releases a client/server's model.
        // Keep no raw model pointers: map changes may flush the loader slots.
        if (modelloader) modelloader->ReleaseAllModels(IModelLoader::FMODELLOADER_GAMEUI);
        m_ModelPath[0] = 0;
        m_Status = "Select an installed model";
    }

    virtual const char *GetStatusText() { return m_Status; }

    virtual bool Render( int width, int height, float yaw, float elapsed )
    {
        if (!m_ModelPath[0] || width <= 0 || height <= 0 || !g_pStudioRender ||
            !materialSystemInterface || !modelloader) return false;
        model_t *model = modelloader->GetModelForName(m_ModelPath, IModelLoader::FMODELLOADER_GAMEUI);
        if (!model || model->type != mod_studio || !model->studio.studiomeshLoaded ||
            !model->studio.hardwareData.m_pLODs || strcmp(model->name, m_ModelPath))
        {
            m_Status = "Model geometry could not be loaded";
            return false;
        }
        studiohdr_t *header = (studiohdr_t *)modelloader->GetExtraData(model);
        if (!header || header->numbones <= 0 || header->numbones > MAXSTUDIOBONES ||
            header->numseq <= 0 || header->numposeparameters > MAXSTUDIOPOSEPARAM) return false;

        // Prefer a genuine idle activity or label, never animate an arbitrary
        // attack/death sequence simply because it is sequence zero.
        int sequence = -1;
        int fallback = -1;
        for (int i = 0; i < header->numseq; ++i)
        {
            mstudioseqdesc_t *seq = header->pSeqdesc(i);
            if (!Q_stricmp(seq->pszActivityName(), "ACT_IDLE")) { sequence = i; break; }
            if (fallback < 0 && (seq->flags & STUDIO_LOOPING) &&
                (Q_stristr(seq->pszLabel(), "idle") || !Q_stricmp(seq->pszLabel(), "stand"))) fallback = i;
        }
        if (sequence < 0) sequence = fallback;
        float poseParameters[MAXSTUDIOPOSEPARAM] = { 0 };
        for (int p = 0; p < header->numposeparameters; ++p)
            Studio_SetPoseParameter(header, p, 0.0f, poseParameters[p]);
        float duration = sequence >= 0 ? Studio_Duration(header, sequence, poseParameters) : 0.0f;
        float cycle = HL2ModernModelPreview::AnimationCycle(elapsed, duration);
        Vector positions[MAXSTUDIOBONES];
        Quaternion rotations[MAXSTUDIOBONES];
        InitPose(header, positions, rotations);
        if (sequence >= 0)
        {
            CalcPose(header, NULL, positions, rotations, sequence, cycle, poseParameters, BONE_USED_BY_ANYTHING);
            CalcAutoplaySequences(header, NULL, positions, rotations, poseParameters, BONE_USED_BY_ANYTHING, elapsed);
        }

        // Frame a sphere containing the model/idle sequence bounds. A single
        // world-space scale and an aspect-correct projection preserve shape.
        Vector mins = header->view_bbmin, maxs = header->view_bbmax;
        if ((maxs - mins).LengthSqr() < 1.0f) { mins = header->hull_min; maxs = header->hull_max; }
        mstudioseqdesc_t *idle = sequence >= 0 ? header->pSeqdesc(sequence) : NULL;
        for (int axis = 0; idle && axis < 3; ++axis)
        {
            mins[axis] = min(mins[axis], idle->bbmin[axis]);
            maxs[axis] = max(maxs[axis], idle->bbmax[axis]);
        }
        const Vector center = (mins + maxs) * 0.5f;
        const float radius = max(1.0f, (maxs - mins).Length() * 0.5f);
        const float aspect = (float)width / (float)height;
        const float distance = HL2ModernModelPreview::CameraDistance(radius, width, height);
        matrix3x4_t rotation;
        AngleMatrix(QAngle(0, yaw, 0), rotation);
        Vector rotatedCenter;
        VectorRotate(center, rotation, rotatedCenter);
        const Vector origin = Vector(distance, 0, 0) - rotatedCenter;
        Studio_BuildMatrices(header, QAngle(0, yaw, 0), origin, positions, rotations,
            -1, g_pStudioRender->GetBoneToWorldArray(), BONE_USED_BY_ANYTHING);

        IMaterialSystem *materials = materialSystemInterface;
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
        materials->LoadIdentity();
        materials->PerspectiveX(40.0, aspect, 1.0, distance + radius * 3.0f);
        materials->MatrixMode(MATERIAL_VIEW);
        materials->LoadIdentity();
        // Same engine/tool convention: camera looks along +X, Z is up.
        materials->Rotate(-90, 1, 0, 0);
        materials->Rotate(-90, 0, 0, 1);
        materials->MatrixMode(MATERIAL_MODEL);
        materials->LoadIdentity();
        // The D3D backend clears only the current viewport's depth rectangle.
        materials->Flush();
        materials->ClearBuffers(false, true);

        g_pStudioRender->SetViewState(vec3_origin, Vector(0,1,0), Vector(0,0,1), Vector(1,0,0));
        g_pStudioRender->SetEyeViewTarget(vec3_origin);
        LightDesc_t light;
        memset(&light, 0, sizeof(light));
        light.m_Type = MATERIAL_LIGHT_DIRECTIONAL;
        light.m_Direction = Vector(-0.6f, -0.4f, -0.7f);
        VectorNormalize(light.m_Direction);
        light.m_Color = Vector(0.65f, 0.65f, 0.65f);
        light.m_Attenuation0 = 1.0f;
        light.m_Range = 2000;
        g_pStudioRender->SetLocalLights(1, &light);
        CUtlVector<Vector> ambient, total;
        const int samples = g_pStudioRender->GetNumAmbientLightSamples();
        ambient.SetSize(samples); total.SetSize(samples);
        for (int sample = 0; sample < samples; ++sample)
        {
            ambient[sample] = Vector(0.35f,0.35f,0.35f);
            const float diffuse = max(0.0f, -DotProduct(light.m_Direction,
                g_pStudioRender->GetAmbientLightDirections()[sample]));
            total[sample] = ambient[sample] + light.m_Color * diffuse;
        }
        g_pStudioRender->SetAmbientLightColors(ambient.Base(), total.Base());
        float flexes[MAXSTUDIOFLEXDESC] = { 0 };
        const float color[3] = { 1, 1, 1 };
        g_pStudioRender->SetFlexWeights(MAXSTUDIOFLEXDESC, flexes);
        g_pStudioRender->SetColorModulation(color);
        g_pStudioRender->SetAlphaModulation(1.0f);
        g_pStudioRender->ForcedMaterialOverride(NULL);
        DrawModelInfo_t info;
        memset(&info, 0, sizeof(info));
        info.m_pStudioHdr = header;
        info.m_pHardwareData = &model->studio.hardwareData;
        info.m_Decals = STUDIORENDER_DECAL_INVALID;
        info.m_Lod = 0;
        g_pStudioRender->DrawModel(info, origin, &info.m_Lod, NULL);
        materials->Flush();
        materials->SetNumBoneWeights(0);
        if (sequence < 0) m_Status = "No idle animation (bind pose)";
        else m_Status = duration > 0.0f ? "Animated 3D preview - drag to rotate" : "Idle sequence has no animation duration";
        return true;
    }

private:
    char m_ModelPath[100];
    const char *m_Status;
};

EXPOSE_SINGLE_INTERFACE(CGameUIModelPreview, IGameUIModelPreview, GAMEUI_MODEL_PREVIEW_INTERFACE_VERSION);
