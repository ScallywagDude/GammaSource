//========= GammaSource ============//
//
// 2003 terrain deformation (client). Port of cl_dll/c_terrainmodmgr.cpp.
//
//=================================//

#include "cbase.h"
#include "terrainmodmgr_shared.h"
#include "terrainmod.h"
#include "hud_macros.h"
#include "iefx.h"
#include "igamesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void __MsgFunc_TerrainMod( bf_read &msg )
{
	CTerrainModParams params;

	unsigned char type = msg.ReadByte();

	params.m_vCenter.x = msg.ReadFloat();
	params.m_vCenter.y = msg.ReadFloat();
	params.m_vCenter.z = msg.ReadFloat();

	params.m_flRadius = RemapVal( msg.ReadWord(), 0, 65535, MIN_TMOD_RADIUS, MAX_TMOD_RADIUS );

	params.m_vecMin.x = msg.ReadFloat();
	params.m_vecMin.y = msg.ReadFloat();
	params.m_vecMin.z = msg.ReadFloat();

	params.m_vecMax.x = msg.ReadFloat();
	params.m_vecMax.y = msg.ReadFloat();
	params.m_vecMax.z = msg.ReadFloat();

	params.m_flStrength = msg.ReadFloat();
	params.m_Flags = msg.ReadByte();

	if ( type == TMod_Suck && ( params.m_Flags & CTerrainModParams::TMOD_SUCKTONORMAL ) )
	{
		params.m_vNormal.x = msg.ReadFloat();
		params.m_vNormal.y = msg.ReadFloat();
		params.m_vNormal.z = msg.ReadFloat();
	}

	// Apply the decal first because the place where we're applying the decal
	// may not be there if we blow it out first!
	C_BaseEntity *pWorld = cl_entitylist->GetEnt( 0 );
	if ( pWorld && type == TMod_Sphere )
	{
		effects->DecalShoot( effects->Draw_DecalIndexFromName( "decals/tscorch" ), 0, pWorld->GetModel(),
			vec3_origin, vec3_angle, params.m_vCenter, NULL, 0 );
	}

	if ( GammaTerrainMod() )
		GammaTerrainMod()->ApplyTerrainModClient( (TerrainModType)type, params );
}

class CTerrainModMessageHook : public CAutoGameSystem
{
public:
	CTerrainModMessageHook() : CAutoGameSystem( "CTerrainModMessageHook" ) {}
	virtual bool Init()
	{
		usermessages->HookMessage( "TerrainMod", __MsgFunc_TerrainMod );
		return true;
	}
};
static CTerrainModMessageHook s_TerrainModMessageHook;
