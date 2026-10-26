// Read-only material diagnostics for original beta assets and mounted overrides.
#include "cbase.h"
#include "c_baseplayer.h"
#include "filesystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "engine/ivmodelinfo.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "vtf/vtf.h"
#include "tier0/memdbgon.h"

static void GammaMaterialPath( const char *kind, const char *name, const char *extension )
{
	char relative[MAX_PATH], full[MAX_PATH];
	Q_snprintf( relative, sizeof(relative), "materials/%s.%s", name, extension );
	full[0] = 0;
	filesystem->RelativePathToFullPath( relative, "GAME", full, sizeof(full) );
	Msg( "GAMMA_MATERIAL_FILE kind=%s file=%s resolved=%s exists=%d\n", kind, relative, full, filesystem->FileExists(relative,"GAME") );
}

static void GammaMaterialDescribe( IMaterial *material )
{
	if ( !material ) return;
	Msg( "GAMMA_MATERIAL name=%s shader=%s error=%d translucent=%d alphatest=%d decal=%d vertexalpha=%d vertexcolor=%d\n",
		material->GetName(), material->GetShaderName(), IsErrorMaterial(material), material->IsTranslucent(), material->IsAlphaTested(),
		material->GetMaterialVarFlag(MATERIAL_VAR_DECAL), material->GetMaterialVarFlag(MATERIAL_VAR_VERTEXALPHA), material->GetMaterialVarFlag(MATERIAL_VAR_VERTEXCOLOR) );
	GammaMaterialPath( "vmt", material->GetName(), "vmt" );
	static const char *parameters[] = { "$basetexture", "$bumpmap", "$iris", "$glint", "$envmap", "$envmapmask", "$envmapsphere", "$selfillum", "$translucent", "$alphatest", "$alpha", "$color", "$normalmapalphaenvmapmask", "$basealphaenvmapmask" };
	for ( int i = 0; i < ARRAYSIZE(parameters); ++i )
	{
		bool found = false;
		IMaterialVar *var = material->FindVar(parameters[i],&found,false);
		if ( !found || !var->IsDefined() ) continue;
		if ( var->IsTexture() && var->GetTextureValue() )
		{
			ITexture *texture = var->GetTextureValue();
			float sample[3] = {0,0,0}; texture->GetLowResColorSample(0.5f,0.5f,sample);
			Msg( "GAMMA_MATERIAL_TEXTURE var=%s texture=%s error=%d size=%dx%d format=%d flags=%x sample=%.3f,%.3f,%.3f\n", parameters[i],texture->GetName(),texture->IsError(),texture->GetActualWidth(),texture->GetActualHeight(),(int)texture->GetImageFormat(),texture->GetFlags(),sample[0],sample[1],sample[2] );
			GammaMaterialPath( parameters[i], texture->GetName(), "vtf" );
		}
		else Msg( "GAMMA_MATERIAL_VAR var=%s value=%s\n",parameters[i],var->GetStringValue() );
	}
}

CON_COMMAND_F( gamma_material_info, "gamma_material_info <material>: resolved shader, textures, flags and actual mounted files", FCVAR_CHEAT )
{
	if ( args.ArgC() > 1 ) GammaMaterialDescribe(materials->FindMaterial(args[1],TEXTURE_GROUP_OTHER,false));
}

CON_COMMAND_F( gamma_material_model, "gamma_material_model <entity index>: inspect the real render model's material slots", FCVAR_CHEAT )
{
	if ( args.ArgC() < 2 ) return;
	C_BaseEntity *entity = C_BaseEntity::Instance(atoi(args[1]));
	const model_t *model = entity ? entity->GetModel() : NULL;
	if ( !model ) { Warning("GAMMA_MATERIAL_MODEL missing entity/model\n"); return; }
	int count = modelinfo->GetModelMaterialCount(model);
	Msg("GAMMA_MATERIAL_MODEL id=%d model=%s slots=%d\n",entity->entindex(),modelinfo->GetModelName(model),count);
	if ( count < 1 || count > 4096 ) return;
	CUtlVector<IMaterial *> slots; slots.SetCount(count);
	modelinfo->GetModelMaterials(model,count,slots.Base());
	for ( int i = 0; i < count; ++i )
	{
		Msg("GAMMA_MATERIAL_SLOT index=%d\n",i);
		GammaMaterialDescribe(slots[i]);
	}
}

CON_COMMAND_F( gamma_material_trace, "gamma_material_trace [x y z pitch yaw]: inspect an actual hit surface without moving the player", FCVAR_CHEAT )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player ) return;
	Vector start = player->EyePosition(), forward;
	QAngle angles = player->EyeAngles();
	if ( args.ArgC() >= 6 )
	{
		start.Init(atof(args[1]),atof(args[2]),atof(args[3]));
		angles.Init(atof(args[4]),atof(args[5]),0);
	}
	AngleVectors(angles,&forward);
	trace_t trace;
	UTIL_TraceLine(start,start+forward*MAX_TRACE_LENGTH,MASK_SHOT,player,COLLISION_GROUP_NONE,&trace);
	Msg("GAMMA_MATERIAL_HIT fraction=%.4f id=%d surface=%s position=%.2f,%.2f,%.2f\n",trace.fraction,trace.m_pEnt?trace.m_pEnt->entindex():-1,trace.surface.name,trace.endpos.x,trace.endpos.y,trace.endpos.z);
	if ( trace.fraction < 1.0f && trace.surface.name ) GammaMaterialDescribe(materials->FindMaterial(trace.surface.name,TEXTURE_GROUP_WORLD,false));
}

CON_COMMAND_F( gamma_vtf_check, "gamma_vtf_check <materials/path.vtf> [mip skip]: native loader, disk byte verification and v7.5 roundtrip", FCVAR_CHEAT )
{
	if ( args.ArgC() < 2 ) return;
	CUtlBuffer input;
	if ( !filesystem->ReadFile(args[1],"GAME",input) )
	{
		Warning("GAMMA_VTF_CHECK missing=%s\n",args[1]); return;
	}
	IVTFTexture *texture = CreateVTFTexture();
	bool header = texture->Unserialize(input,true);
	int skip = args.ArgC() > 2 ? clamp(atoi(args[2]),0,MAX(0,texture->MipCount()-1)) : 0;
	input.SeekGet(CUtlBuffer::SEEK_HEAD,0);
	bool loaded = header && texture->Unserialize(input,false,skip);
	bool diskMatch = loaded;
	const int faces = loaded && texture->IsCubeMap() ? 6 : texture->FaceCount();
	if ( loaded ) for ( int frame = 0; frame < texture->FrameCount(); ++frame )
		for ( int face = 0; face < faces; ++face )
			for ( int mip = 0; mip < texture->MipCount(); ++mip )
			{
				int offset, bytes; texture->ImageFileInfo(frame,face,mip,&offset,&bytes);
				if ( offset < 0 || bytes < 0 || offset > input.TellMaxPut()-bytes ||
					memcmp((const byte *)input.Base()+offset,texture->ImageData(frame,face,mip),bytes) ) diskMatch = false;
			}
	CUtlBuffer output;
	IVTFTexture *roundtrip = CreateVTFTexture();
	bool roundtripMatch = loaded && texture->Serialize(output) && roundtrip->Unserialize(output);
	if ( roundtripMatch )
	{
		roundtripMatch = texture->Width()==roundtrip->Width() && texture->Height()==roundtrip->Height() && texture->MipCount()==roundtrip->MipCount() && texture->FrameCount()==roundtrip->FrameCount();
		for ( int frame = 0; roundtripMatch && frame < texture->FrameCount(); ++frame )
			for ( int face = 0; roundtripMatch && face < faces; ++face )
				for ( int mip = 0; roundtripMatch && mip < texture->MipCount(); ++mip )
					if ( memcmp(texture->ImageData(frame,face,mip),roundtrip->ImageData(frame,face,mip),texture->ComputeMipSize(mip)) ) roundtripMatch=false;
	}
	Msg("GAMMA_VTF_CHECK file=%s skip=%d loaded=%d disk_match=%d roundtrip=%d source_bytes=%d size=%dx%d faces=%d mips=%d serialized_bytes=%d\n",args[1],skip,loaded,diskMatch,roundtripMatch,input.TellMaxPut(),texture->Width(),texture->Height(),faces,texture->MipCount(),output.TellMaxPut());
	DestroyVTFTexture(roundtrip);
	DestroyVTFTexture(texture);
}
