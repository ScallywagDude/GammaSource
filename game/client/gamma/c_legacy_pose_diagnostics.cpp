// Read-only snapshots for comparing original model poses with native rendering.
#include "cbase.h"
#include "c_baseanimating.h"
#include "c_baseanimatingoverlay.h"
#include "animationlayer.h"
#include "engine/ivmodelinfo.h"
#include "tier1/utlstring.h"
#include "tier0/memdbgon.h"

static ConVar gamma_legacy_pose_debug( "gamma_legacy_pose_debug", "0", FCVAR_CHEAT,
	"Enable read-only gamma_pose_snapshot JSON console records." );

static CUtlString GammaPoseQuoted( const char *text )
{
	CUtlString result( "\"" );
	for ( const unsigned char *c = (const unsigned char *)( text ? text : "" ); *c; ++c )
	{
		char escaped[8];
		if ( *c == '\\' || *c == '"' )
			Q_snprintf( escaped, sizeof(escaped), "\\%c", *c );
		else if ( *c < 32 )
			Q_snprintf( escaped, sizeof(escaped), "\\u%04x", *c );
		else { escaped[0] = *c; escaped[1] = 0; }
		result += escaped;
	}
	result += "\"";
	return result;
}

static void GammaPoseFloats( const float *values, int count )
{
	Msg( "[" );
	for ( int i = 0; i < count; ++i )
	{
		if ( i ) Msg( "," );
		if ( IsFinite(values[i]) ) Msg( "%.9g", values[i] );
		else Msg( "null" );
	}
	Msg( "]" );
}

CON_COMMAND_F( gamma_pose_snapshot, "gamma_pose_snapshot <entity index> [label]: dump current client pose, overlays and final bone matrices", FCVAR_CHEAT )
{
	if ( !gamma_legacy_pose_debug.GetBool() || args.ArgC() < 2 )
	{
		Msg( "GAMMA_POSE_DISABLED set gamma_legacy_pose_debug 1, then gamma_pose_snapshot <entity index> [label]\n" );
		return;
	}
	C_BaseEntity *entity = C_BaseEntity::Instance( atoi(args[1]) );
	C_BaseAnimating *actor = entity ? entity->GetBaseAnimating() : NULL;
	CStudioHdr *hdr = actor ? actor->GetModelPtr() : NULL;
	if ( !hdr || hdr->numbones() < 1 || hdr->numbones() > MAXSTUDIOBONES )
	{
		Warning( "GAMMA_POSE_ERROR missing animated entity/model\n" );
		return;
	}
	static unsigned int serial = 0;
	const unsigned int snapshot = ++serial;
	const int sequence = actor->GetSequence();
	const char *sequenceName = sequence >= 0 && sequence < hdr->GetNumSeq() ? hdr->pSeqdesc(sequence).pszLabel() : "<invalid>";
	matrix3x4_t matrices[MAXSTUDIOBONES];
	const bool setup = actor->SetupBones( matrices, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
	Msg( "GAMMA_POSE {\"record\":\"begin\",\"snapshot\":%u,\"label\":%s,\"id\":%d,\"time\":%.9g,\"frame\":%d,\"model\":%s,\"sequence\":%d,\"sequence_name\":%s,\"cycle\":%.9g,\"playback_rate\":%.9g,\"setup_bones\":%s,\"origin\":",
		snapshot, GammaPoseQuoted(args.ArgC() > 2 ? args[2] : "").Get(), actor->entindex(), gpGlobals->curtime, gpGlobals->framecount,
		GammaPoseQuoted(modelinfo->GetModelName(actor->GetModel())).Get(), sequence, GammaPoseQuoted(sequenceName).Get(), actor->GetCycle(), actor->GetPlaybackRate(), setup ? "true" : "false" );
	GammaPoseFloats( actor->GetRenderOrigin().Base(), 3 );
	Msg( ",\"angles\":" ); GammaPoseFloats( actor->GetRenderAngles().Base(), 3 );
	Msg( "}\n" );
	float poses[MAXSTUDIOPOSEPARAM];
	actor->GetPoseParameters( hdr, poses );
	for ( int i = 0; i < hdr->GetNumPoseParameters() && i < MAXSTUDIOPOSEPARAM; ++i )
	{
		const mstudioposeparamdesc_t &pose = hdr->pPoseParameter(i);
		Msg( "GAMMA_POSE {\"record\":\"pose_parameter\",\"snapshot\":%u,\"index\":%d,\"name\":%s,\"normalized\":%.9g,\"start\":%.9g,\"end\":%.9g,\"loop\":%.9g}\n",
			snapshot, i, GammaPoseQuoted(pose.pszName()).Get(), poses[i], pose.start, pose.end, pose.loop );
	}
	C_BaseAnimatingOverlay *overlay = dynamic_cast<C_BaseAnimatingOverlay *>(actor);
	for ( int i = 0; overlay && i < overlay->GetNumAnimOverlays(); ++i )
	{
		C_AnimationLayer *layer = overlay->GetAnimOverlay(i);
		const int layerSequence = layer->m_nSequence;
		const char *layerName = layerSequence >= 0 && layerSequence < hdr->GetNumSeq() ? hdr->pSeqdesc(layerSequence).pszLabel() : "<invalid>";
		Msg( "GAMMA_POSE {\"record\":\"overlay\",\"snapshot\":%u,\"index\":%d,\"order\":%d,\"sequence\":%d,\"sequence_name\":%s,\"cycle\":%.9g,\"weight\":%.9g,\"previous_cycle\":%.9g,\"client_blend\":%s}\n",
			snapshot, i, layer->m_nOrder, layerSequence, GammaPoseQuoted(layerName).Get(), (float)layer->m_flCycle,
			(float)layer->m_flWeight, (float)layer->m_flPrevCycle, layer->m_bClientBlend ? "true" : "false" );
	}
	if ( setup )
	{
		for ( int i = 0; i < hdr->numbones(); ++i )
		{
			const mstudiobone_t *bone = hdr->pBone(i);
			Msg( "GAMMA_POSE {\"record\":\"bone\",\"snapshot\":%u,\"index\":%d,\"name\":%s,\"parent\":%d,\"flags\":%d,\"procedure_type\":%d,\"procedure_offset\":%d,\"matrix\":",
				snapshot, i, GammaPoseQuoted(bone->pszName()).Get(), bone->parent, bone->flags, bone->proctype, bone->procindex );
			GammaPoseFloats( &matrices[i][0][0], 12 );
			Msg( "}\n" );
		}
	}
	Msg( "GAMMA_POSE {\"record\":\"end\",\"snapshot\":%u,\"bones\":%d}\n", snapshot, setup ? hdr->numbones() : 0 );
}
