#ifndef HL2_MODERN_PREVIEW_STUDIO_VALIDATION_H
#define HL2_MODERN_PREVIEW_STUDIO_VALIDATION_H

// Defensive preflight before the 2003 model loader and animation code follow
// relative MDL pointers. This is a format check, not visual certification.
#include "mathlib.h"
#include "vector4d.h"
#include "studio.h"
#include <string.h>

namespace HL2ModernModelPreview
{
class StudioDataValidator
{
public:
    StudioDataValidator(const void *data, int length) : m_Data((const byte *)data), m_Length(length) {}
    bool Range(int offset, int count, int stride) const
    {
        if (count == 0) return true;
        return offset >= (int)sizeof(studiohdr_t) && offset <= m_Length &&
            count > 0 && stride > 0 && count <= (m_Length - offset) / stride;
    }
    int Offset(const void *owner, int relative) const
    {
        const int start = (int)((const byte *)owner - m_Data);
        if (start < 0 || start > m_Length || relative < -start || relative > m_Length - start)
            return -1;
        return start + relative;
    }
    bool RelativeRange(const void *owner, int relative, int count, int stride) const
    {
        return count == 0 || Range(Offset(owner,relative), count,stride);
    }
    bool String(const void *owner, int relative) const
    {
        const int at = Offset(owner, relative);
        return at >= (int)sizeof(studiohdr_t) && at < m_Length &&
            memchr(m_Data + at, 0, m_Length - at) != NULL;
    }
    // HL2MODERN_STUDIO_LAYOUT_V37_V21: a v37-labelled blob is accepted ONLY
    // if all the pointer tables, sequence channels, meshes and strings can
    // be walked with this tree's v36 structure definitions. This does NOT
    // implement every distinct v37 layout found in later models.
    bool Validate(bool allowV37Layout = false) const
    {
        if (!m_Data || m_Length < sizeof(studiohdr_t)) return false;
        const studiohdr_t *h = (const studiohdr_t *)m_Data;
        // HL2MODERN_STUDIO_STATIC_PROP_V21: the original format explicitly
        // permits static props without bones or animation channels. Reject
        // invalid IDs and negative counts without discarding valid props.
        const bool staticProp = (h->flags & STUDIOHDR_FLAGS_STATIC_PROP) != 0;
        if (h->id != (('T'<<24)+('S'<<16)+('D'<<8)+'I') ||
            (h->version != STUDIO_VERSION &&
             (!allowV37Layout || h->version != 37)) || h->length != m_Length ||
            h->numbones < (staticProp ? 0 : 1) || h->numbones > MAXSTUDIOBONES ||
            h->numseq < 0 || h->numanim < 0 ||
            (!staticProp && (!h->numseq || !h->numanim)) ||
            h->numbodyparts <= 0 ||
            h->numposeparameters < 0 || h->numposeparameters > MAXSTUDIOPOSEPARAM ||
            h->numflexdesc < 0 || h->numflexdesc > MAXSTUDIOFLEXDESC)
            return false;
        if (!Range(h->boneindex,h->numbones,sizeof(mstudiobone_t)) ||
            !Range(h->bonecontrollerindex,h->numbonecontrollers,sizeof(mstudiobonecontroller_t)) ||
            !Range(h->animdescindex,h->numanim,sizeof(mstudioanimdesc_t)) ||
            !Range(h->seqindex,h->numseq,sizeof(mstudioseqdesc_t)) ||
            !Range(h->bodypartindex,h->numbodyparts,sizeof(mstudiobodyparts_t)) ||
            !Range(h->textureindex,h->numtextures,sizeof(mstudiotexture_t)) ||
            !Range(h->cdtextureindex,h->numcdtextures,sizeof(int)) ||
            !Range(h->poseparamindex,h->numposeparameters,sizeof(mstudioposeparamdesc_t)) ||
            !Range(h->hitboxsetindex,h->numhitboxsets,sizeof(mstudiohitboxset_t)) ||
            !Range(h->attachmentindex,h->numattachments,sizeof(mstudioattachment_t)) ||
            !Range(h->flexdescindex,h->numflexdesc,sizeof(mstudioflexdesc_t)) ||
            !Range(h->flexcontrollerindex,h->numflexcontrollers,sizeof(mstudioflexcontroller_t)) ||
            !Range(h->flexruleindex,h->numflexrules,sizeof(mstudioflexrule_t)) ||
            !Range(h->ikchainindex,h->numikchains,sizeof(mstudioikchain_t))) return false;
        if (h->numskinref < 0 || h->numskinfamilies < 0 || h->numskinref > 65536 ||
            h->numskinfamilies > 65536 || (h->numskinref &&
            h->numskinfamilies > m_Length / (int)sizeof(short) / h->numskinref) ||
            !Range(h->skinindex,h->numskinref*h->numskinfamilies,sizeof(short))) return false;
        for (int bone = 0; bone < h->numbones; ++bone)
        {
            const mstudiobone_t *b = h->pBone(bone);
            if (b->parent < -1 || b->parent >= bone || !String(b,b->sznameindex)) return false;
        }
        for (int p = 0; p < h->numposeparameters; ++p)
            if (!String(h->pPoseParameter(p),h->pPoseParameter(p)->sznameindex)) return false;
        for (int anim = 0; anim < h->numanim; ++anim)
        {
            const mstudioanimdesc_t *a = h->pAnimdesc(anim);
            if (a->numframes <= 0 || a->numframes > 1000000 || !String(a,a->sznameindex) ||
                !RelativeRange(a,a->animindex,h->numbones,sizeof(mstudioanim_t)) ||
                !RelativeRange(a,a->movementindex,a->nummovements,sizeof(mstudiomovement_t))) return false;
            for (int b = 0; b < h->numbones; ++b)
            {
                const mstudioanim_t *channel = a->pAnim(b);
                for (int dof = 0; dof < 6; ++dof)
                {
                    const int flag = dof < 3 ? STUDIO_POS_ANIMATED : STUDIO_ROT_ANIMATED;
                    if (!(channel->flags & flag) || !channel->u.offset[dof]) continue;
                    int at = Offset(channel,channel->u.offset[dof]);
                    int frames = 0;
                    while (frames < a->numframes)
                    {
                        if (!Range(at,1,sizeof(mstudioanimvalue_t))) return false;
                        const mstudioanimvalue_t *span = (const mstudioanimvalue_t *)(m_Data + at);
                        if (!span->num.total || !span->num.valid || span->num.valid > span->num.total ||
                            !Range(at,span->num.valid+1,sizeof(mstudioanimvalue_t))) return false;
                        frames += span->num.total;
                        at += (span->num.valid+1) * sizeof(mstudioanimvalue_t);
                    }
                }
            }
        }
        for (int seq = 0; seq < h->numseq; ++seq)
        {
            const mstudioseqdesc_t *s = h->pSeqdesc(seq);
            if (!String(s,s->szlabelindex) || (s->szactivitynameindex && !String(s,s->szactivitynameindex)) ||
                !RelativeRange(s,s->weightlistindex,h->numbones,sizeof(float)) ||
                !RelativeRange(s,s->autolayerindex,s->numautolayers,sizeof(mstudioautolayer_t)) ||
                s->groupsize[0] <= 0 || s->groupsize[0] > MAXSTUDIOBLENDS ||
                s->groupsize[1] <= 0 || s->groupsize[1] > MAXSTUDIOBLENDS) return false;
            for (int axis = 0; axis < 2; ++axis)
                if (s->paramindex[axis] < -1 || s->paramindex[axis] >= h->numposeparameters) return false;
            if (s->posekeyindex && !RelativeRange(s,s->posekeyindex,s->groupsize[0]+s->groupsize[1],sizeof(float))) return false;
            for (int x = 0; x < s->groupsize[0]; ++x)
                for (int y = 0; y < s->groupsize[1]; ++y)
                    if (s->anim[x][y] < 0 || s->anim[x][y] >= h->numanim) return false;
            for (int layer = 0; layer < s->numautolayers; ++layer)
                if (s->pAutolayer(layer)->iSequence < 0 || s->pAutolayer(layer)->iSequence >= h->numseq ||
                    s->pAutolayer(layer)->iSequence == seq) return false;
        }
        for (int t = 0; t < h->numtextures; ++t)
            if (!String(h->pTexture(t),h->pTexture(t)->sznameindex)) return false;
        for (int d = 0; d < h->numcdtextures; ++d)
        {
            int relative = ((const int *)(m_Data+h->cdtextureindex))[d];
            if (!String(h,relative)) return false;
        }
        for (int body = 0; body < h->numbodyparts; ++body)
        {
            const mstudiobodyparts_t *part = h->pBodypart(body);
            if (part->base <= 0 || part->nummodels <= 0 || !String(part,part->sznameindex) ||
                !RelativeRange(part,part->modelindex,part->nummodels,sizeof(mstudiomodel_t))) return false;
            for (int submodel = 0; submodel < part->nummodels; ++submodel)
            {
                const mstudiomodel_t *m = part->pModel(submodel);
                if (m->numvertices < 0 || m->numvertices >= MAXSTUDIOVERTS ||
                    !RelativeRange(m,m->meshindex,m->nummeshes,sizeof(mstudiomesh_t)) ||
                    !RelativeRange(m,m->vertexindex,m->numvertices,sizeof(mstudiovertex_t)) ||
                    (m->tangentsindex && !RelativeRange(m,m->tangentsindex,m->numvertices,sizeof(Vector4D))) ||
                    !RelativeRange(m,m->eyeballindex,m->numeyeballs,sizeof(mstudioeyeball_t))) return false;
                for (int mesh = 0; mesh < m->nummeshes; ++mesh)
                {
                    const mstudiomesh_t *item = m->pMesh(mesh);
                    if (item->material < 0 || item->material >= h->numskinref ||
                        item->numvertices < 0 || item->vertexoffset < 0 ||
                        item->vertexoffset > m->numvertices || item->numvertices > m->numvertices-item->vertexoffset ||
                        Offset(item,item->modelindex) != (const byte *)m-m_Data ||
                        !RelativeRange(item,item->flexindex,item->numflexes,sizeof(mstudioflex_t))) return false;
                }
            }
        }
        return true;
    }
private:
    const byte *m_Data;
    int m_Length;
};
}
#endif
