#!/usr/bin/env python3
"""Tighten v37 MDL validation without excluding legitimate static props."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'modernize/model_preview_studio_validation.h'
s=p.read_text()
old='''        if ((h->version != STUDIO_VERSION &&
             (!allowV37Layout || h->version != 37)) || h->length != m_Length ||
            h->numbones <= 0 || h->numbones > MAXSTUDIOBONES ||
            h->numseq <= 0 || h->numanim <= 0 || h->numbodyparts <= 0 ||'''
new='''        // HL2MODERN_STUDIO_STATIC_PROP_V21: the original format explicitly
        // permits static props without bones or animation channels. Reject
        // invalid IDs and negative counts without discarding valid props.
        const bool staticProp = (h->flags & STUDIOHDR_FLAGS_STATIC_PROP) != 0;
        if (h->id != IDSTUDIOHEADER ||
            (h->version != STUDIO_VERSION &&
             (!allowV37Layout || h->version != 37)) || h->length != m_Length ||
            h->numbones < (staticProp ? 0 : 1) || h->numbones > MAXSTUDIOBONES ||
            h->numseq < 0 || h->numanim < 0 ||
            (!staticProp && (!h->numseq || !h->numanim)) ||
            h->numbodyparts <= 0 ||'''
if 'HL2MODERN_STUDIO_STATIC_PROP_V21' not in s:
    assert s.count(old)==1
    p.write_text(s.replace(old,new))
print('v21 studio validator static-prop/ID checks applied')
