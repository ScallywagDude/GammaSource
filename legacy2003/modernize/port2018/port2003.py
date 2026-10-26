"""Mechanically port a 2003 game-DLL source file to the 2018 game code conventions.
usage: port2003.py <src> <dst> [--banner text]
Only does the repetitive renames; the rest is fixed by hand while compiling."""
import re, sys
from pathlib import Path

src, dst = Path(sys.argv[1]), Path(sys.argv[2])
t = src.read_text(encoding='latin-1').replace('\r\n', '\n')

# datadesc macros lost their class argument
t = re.sub(r'\bDEFINE_(EMBEDDED_AUTO_ARRAY|EMBEDDED_ARRAY|FIELD|KEYFIELD|INPUTFUNC|OUTPUT|ARRAY|AUTO_ARRAY|EMBEDDED|SOUNDPATCH|PHYSPTR|THINKFUNC|ENTITYFUNC|USEFUNC|INPUT|CUSTOM_FIELD|UTLVECTOR|SIMTIMER|FIELD_NAME|KEYFIELD_NOT_SAVED|EMBEDDED_PTR)\(\s*\w+\s*,\s*',
           r'DEFINE_\1( ', t)

# DEFINE_FUNCTION( Class, Fn ): 2018 wants THINKFUNC / ENTITYFUNC / USEFUNC by signature
context = t
for extra in sys.argv[3:]:
    p = Path(extra)
    if p.exists():
        context += p.read_text(encoding='latin-1')
hdr = src.with_suffix('.h')
if hdr.exists():
    context += hdr.read_text(encoding='latin-1')

def func_kind(name):
    m = re.search(r'\b(?:void|bool)\s+(?:\w+::)?' + re.escape(name) + r'\s*\(([^)]*)\)', context)
    args = m.group(1) if m else ''
    if 'USE_TYPE' in args:
        return 'USEFUNC'
    if 'CBaseEntity' in args:
        return 'ENTITYFUNC'
    if 'inputdata_t' in args:
        return 'FUNCTION'
    return 'THINKFUNC'

t = re.sub(r'\bDEFINE_FUNCTION\(\s*\w+\s*,\s*(\w+)\s*\)', lambda m: 'DEFINE_%s( %s )' % (func_kind(m.group(1)), m.group(1)), t)

# API drift 2003 -> 2018
t = re.sub(r'(\w+(?:->|\.))m_fEffects\s*\|=\s*EF_MUZZLEFLASH\s*;', r'\1DoMuzzleFlash();', t)
t = re.sub(r'\bm_fEffects\s*\|=\s*EF_MUZZLEFLASH\s*;', 'DoMuzzleFlash();', t)
t = re.sub(r'(\w+(?:->|\.))m_fEffects\s*\|=\s*(EF_\w+)\s*;', r'\1AddEffects( \2 );', t)
t = re.sub(r'(\w+(?:->|\.))m_fEffects\s*&=\s*~\s*(EF_\w+)\s*;', r'\1RemoveEffects( \2 );', t)
t = re.sub(r'\bm_fEffects\s*\|=\s*(EF_\w+)\s*;', r'AddEffects( \1 );', t)
t = re.sub(r'\bm_fEffects\s*&=\s*~\s*(EF_\w+)\s*;', r'RemoveEffects( \1 );', t)
t = re.sub(r'\bm_fEffects\s*=\s*([^;=][^;]*);', r'SetEffects( \1 );', t)
t = re.sub(r'\bm_fEffects\s*&\s*(EF_\w+)', r'IsEffectActive( \1 )', t)
t = re.sub(r'->SetFOV\(\s*(?!this\b)', '->SetFOV( this, ', t)
t = re.sub(r'\bint(\s+(?:\w+::)?Get(?:Primary|Secondary)AttackActivity\s*\()', r'Activity\1', t)
t = re.sub(r'^\s*Relink\(\s*\)\s*;\s*\n', '', t, flags=re.M)
t = re.sub(r'(\w+(?:->|\.))?GetAbsMins\(\)', lambda m: '( %sGetAbsOrigin() + %sCollisionProp()->OBBMins() )' % ((m.group(1) or ''), (m.group(1) or '')), t)
t = re.sub(r'(\w+(?:->|\.))?GetAbsMaxs\(\)', lambda m: '( %sGetAbsOrigin() + %sCollisionProp()->OBBMaxs() )' % ((m.group(1) or ''), (m.group(1) or '')), t)
t = re.sub(r'\bEntitySpaceMins\(\)', 'CollisionProp()->OBBMins()', t)
t = re.sub(r'\bEntitySpaceMaxs\(\)', 'CollisionProp()->OBBMaxs()', t)
t = re.sub(r'\btrace_t\s+(\w+)\s*=\s*((?:CBaseEntity::)?GetTouchTrace\(\s*\))', r'trace_t \1; \1 = \2', t)
t = re.sub(r'\bm_flCycle\s*=\s*([^;]+);', r'SetCycle( \1 );', t)
t = re.sub(r'\bRadiusDamage\s*\(([^;]*?),\s*(CLASS_\w+)\s*\)\s*;', r'RadiusDamage(\1, \2, NULL );', t)
# AI framework drift
t = re.sub(r'\bm_pHintNode\s*=\s*([^;]+);', r'SetHintNode( \1 );', t)
t = re.sub(r'\bm_pHintNode\b', 'GetHintNode()', t)
t = re.sub(r'\bCAI_Hint::FindHint\(\s*this\s*,\s*([^,]+),', r'CAI_HintManager::FindHint( this, (Hint_e)( \1 ),', t)
t = re.sub(r'\bm_IdealNPCState\s*=\s*([^;=][^;]*);', r'SetIdealState( \1 );', t)
t = re.sub(r'\bm_IdealNPCState\b', 'GetIdealState()', t)
t = re.sub(r'(TraceAttack\(\s*const CTakeDamageInfo\s*&\s*\w+\s*,\s*const Vector\s*&\s*vecDir\s*,\s*trace_t\s*\*\s*ptr)\s*\)', r'\1, CDmgAccumulator *pAccumulator )', t)
t = re.sub(r'(BaseClass::TraceAttack\(\s*\w+\s*,\s*vecDir\s*,\s*ptr)\s*\)', r'\1, pAccumulator )', t)
t = re.sub(r'^\s*UTIL_Relink\([^;]*\);\s*\n', '', t, flags=re.M)
t = re.sub(r'CEntityMessageFilter\s+(\w+)\(\s*this\s*,\s*"[^"]*"\s*\);\s*\n(\s*)MessageBegin\(\s*\1\s*,\s*0\s*\);', r'EntityMessageBegin( this );', t)
t = re.sub(r'FBitSet\(\s*([\w\->\.]+?)(?:->|\.)m_fEffects\s*,\s*(EF_\w+)\s*\)', r'\1->IsEffectActive( \2 )', t)
t = re.sub(r'FollowEntity\(\s*entindex\(\)\s*\)', 'FollowEntity( this )', t)
t = re.sub(r'\bm_nKickDamage\s*=\s*([^;]+);', r'SetKickDamage( \1 );', t)
# 2018 moved render state calls from IMaterialSystem to IMatRenderContext (render-context calls)
t = re.sub(r'\bmaterials->(GetDynamicMesh|Bind|MatrixMode|PushMatrix|PopMatrix|LoadIdentity|LoadMatrix|MultMatrix|Translate|Rotate|Scale|CullMode|DepthRange|Viewport|GetViewport|SetLight|SetAmbientLight|ClearBuffers|ReadPixels|Flush|BindLightmapPage|SetRenderTarget|GetRenderTarget|CopyRenderTargetToTexture|DrawScreenSpaceQuad|FogMode|FogStart|FogEnd|FogColor3f|FogColor3ub|SetHeightClipMode|OverrideDepthEnable)\(', r'CMatRenderContextPtr( materials )->\1(', t)
t = re.sub(r'\bHolster\(\s*\)', 'Holster( NULL )', t)
t = re.sub(r'(?<![\w:])min\(', 'MIN(', t)
t = re.sub(r'(?<![\w:])max\(', 'MAX(', t)
t = t.replace('PRECACHE_SOUND_ARRAY(', 'GAMMA_PRECACHE_SOUND_ARRAY(')
if 'GAMMA_PRECACHE_SOUND_ARRAY(' in t and '#define GAMMA_PRECACHE_SOUND_ARRAY' not in t:
    t = t.replace('#include "cbase.h"\n', '#include "cbase.h"\n\n// 2003 helper, gone from the 2018 code\n#define GAMMA_PRECACHE_SOUND_ARRAY( a ) { for ( int i_ = 0; i_ < ARRAYSIZE( a ); i_++ ) PrecacheSound( (char *)a[i_] ); }\n', 1)

# header renames
renames = {
    '"AI_BaseNPC.h"': '"ai_basenpc.h"', '"mathlib.h"': '"mathlib/mathlib.h"', '"vector.h"': '"mathlib/vector.h"',
    '"vmatrix.h"': '"mathlib/vmatrix.h"', '"BaseBludgeonWeapon.h"': '"basebludgeonweapon.h"', '"NPCevent.h"': '"npcevent.h"',
    '"NPCEvent.h"': '"npcevent.h"', '"AmmoDef.h"': '"ammodef.h"', '"IEffects.h"': '"IEffects.h"',
}
# CASE_INSENSITIVE_HEADERS: 2003 code spells these headers many ways
for hname, hnew in (('vector.h', 'mathlib/vector.h'), ('vmatrix.h', 'mathlib/vmatrix.h'), ('mathlib.h', 'mathlib/mathlib.h'), ('vector2d.h', 'mathlib/vector2d.h'), ('vector4d.h', 'mathlib/vector4d.h')):
    t = re.sub(r'#include\s+"' + re.escape(hname) + r'"', '#include "' + hnew + '"', t, flags=re.I)
for a, b in renames.items():
    t = t.replace('#include ' + a, '#include ' + b)

# entity fields that became private / accessors
t = re.sub(r'\bm_spawnflags\s*&\s*(\w+)', r'HasSpawnFlags( \1 )', t)
t = re.sub(r'\bpev->', '', t)

banner = '//========= GammaSource: ported from the 2003 code (%s) ============//\n' % src.as_posix().split('GammaSource-main/')[-1]
t = re.sub(r'^//=+ Copyright[^\n]*\n', banner, t, count=1)
if 'memdbgon.h' not in t:
    # memdbgon goes after the last include
    incs = list(re.finditer(r'^#include[^\n]*\n', t, re.M))
    if incs:
        e = incs[-1].end()
        t = t[:e] + '\n// memdbgon must be the last include file in a .cpp file!!!\n#include "tier0/memdbgon.h"\n' + t[e:] if dst.suffix == '.cpp' else t
dst.parent.mkdir(parents=True, exist_ok=True)
dst.write_text(t, encoding='latin-1')
print('ported', src.name, '->', dst)
