#include "../model_preview_studio_validation.h"
#include <assert.h>
#include <vector>

int main()
{
    int at = sizeof(studiohdr_t);
    const int boneAt = at; at += sizeof(mstudiobone_t);
    const int animAt = at; at += sizeof(mstudioanimdesc_t);
    const int channelAt = at; at += sizeof(mstudioanim_t);
    const int seqAt = at; at += sizeof(mstudioseqdesc_t);
    const int partAt = at; at += sizeof(mstudiobodyparts_t);
    const int modelAt = at; at += sizeof(mstudiomodel_t);
    const int weightAt = at; at += sizeof(float);
    const int nameAt = at; at += 16;
    std::vector<byte> bytes(at,0);
    studiohdr_t *h = (studiohdr_t *)&bytes[0];
    h->version = STUDIO_VERSION; h->length = at;
    h->numbones = 1; h->boneindex = boneAt;
    h->numanim = 1; h->animdescindex = animAt;
    h->numseq = 1; h->seqindex = seqAt;
    h->numbodyparts = 1; h->bodypartindex = partAt;
    strcpy((char *)&bytes[nameAt],"idle");
    mstudiobone_t *b = h->pBone(0);
    b->parent = -1; b->sznameindex = nameAt-boneAt;
    mstudioanimdesc_t *a = h->pAnimdesc(0);
    a->numframes = 1; a->sznameindex = nameAt-animAt; a->animindex = channelAt-animAt;
    mstudioseqdesc_t *s = h->pSeqdesc(0);
    s->szlabelindex = nameAt-seqAt; s->szactivitynameindex = nameAt-seqAt;
    s->weightlistindex = weightAt-seqAt;
    s->groupsize[0] = s->groupsize[1] = 1;
    s->paramindex[0] = s->paramindex[1] = -1;
    mstudiobodyparts_t *p = h->pBodypart(0);
    p->sznameindex = nameAt-partAt; p->base = 1; p->nummodels = 1; p->modelindex = modelAt-partAt;
    using HL2ModernModelPreview::StudioDataValidator;
    assert(StudioDataValidator(&bytes[0],at).Validate());
    // v37 can enter the engine only if it uses the precise v36 table layout.
    h->version = 37;
    assert(!StudioDataValidator(&bytes[0],at).Validate());
    assert(StudioDataValidator(&bytes[0],at).Validate(true));
    h->version = 38;
    assert(!StudioDataValidator(&bytes[0],at).Validate(true));
    h->version = STUDIO_VERSION;
    h->boneindex = at;
    assert(!StudioDataValidator(&bytes[0],at).Validate());
    h->boneindex = boneAt;
    b->parent = 0;
    assert(!StudioDataValidator(&bytes[0],at).Validate());
    b->parent = -1;
    s->szlabelindex = at;
    assert(!StudioDataValidator(&bytes[0],at).Validate());
    s->szlabelindex = nameAt-seqAt;
    a->animindex = at;
    assert(!StudioDataValidator(&bytes[0],at).Validate());
    a->animindex = channelAt-animAt;
    s->anim[0][0] = 2;
    assert(!StudioDataValidator(&bytes[0],at).Validate());
    s->anim[0][0] = 0;
    p->modelindex = -100000;
    assert(!StudioDataValidator(&bytes[0],at).Validate());
    p->modelindex = modelAt-partAt;
    h->numposeparameters = 1; h->poseparamindex = at;
    assert(!StudioDataValidator(&bytes[0],at).Validate());
    h->numposeparameters = 0;
    mstudioanim_t *channel = a->pAnim(0);
    channel->flags = STUDIO_ROT_ANIMATED;
    channel->u.offset[3] = at-2-channelAt;
    assert(!StudioDataValidator(&bytes[0],at).Validate()); // zero-length RLE span
    return 0;
}
