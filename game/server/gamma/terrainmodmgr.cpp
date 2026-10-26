//========= GammaSource ============//
//
// 2003 terrain deformation (server). Port of dlls/terrainmodmgr.cpp.
//
//=================================//

#include "cbase.h"
#include "terrainmodmgr.h"
#include "terrainmodmgr_shared.h"
#include "player.h"
#include "igamesystem.h"
#include "isaverestore.h"
#include "saverestore.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void EncodeTerrainMod( unsigned char type, const Vector &vCenter, const Vector &vNormal, unsigned short usRadius,
	const Vector &vecMin, const Vector &vecMax, float flStrength, unsigned char ucFlags )
{
	WRITE_BYTE( type );
	WRITE_FLOAT( vCenter.x );
	WRITE_FLOAT( vCenter.y );
	WRITE_FLOAT( vCenter.z );
	WRITE_SHORT( usRadius );
	WRITE_FLOAT( vecMin.x );
	WRITE_FLOAT( vecMin.y );
	WRITE_FLOAT( vecMin.z );
	WRITE_FLOAT( vecMax.x );
	WRITE_FLOAT( vecMax.y );
	WRITE_FLOAT( vecMax.z );
	WRITE_FLOAT( flStrength );
	WRITE_BYTE( ucFlags );

	if ( type == TMod_Suck && ( ucFlags & CTerrainModParams::TMOD_SUCKTONORMAL ) )
	{
		WRITE_FLOAT( vNormal.x );
		WRITE_FLOAT( vNormal.y );
		WRITE_FLOAT( vNormal.z );
	}
}

//-----------------------------------------------------------------------------
// Every mod applied this level, so clients that connect later (or reconnect after
// a load) get the same terrain. 2003 used the signon buffer for this, which the
// 2018 engine caps far lower than a morphing env_terrainmorph needs.
//-----------------------------------------------------------------------------
struct StoredTerrainMod_t
{
	unsigned char type;
	Vector vCenter, vNormal, vecMin, vecMax;
	unsigned short usRadius;
	float flStrength;
	unsigned char ucFlags;
};
static CUtlVector<StoredTerrainMod_t> s_TerrainMods;

class CTerrainModSender : public CAutoGameSystemPerFrame
{
public:
	CTerrainModSender() : CAutoGameSystemPerFrame( "CTerrainModSender" ) {}

	virtual void LevelInitPreEntity() { s_TerrainMods.Purge(); Reset(); }
	virtual void LevelShutdownPostEntity() { s_TerrainMods.Purge(); Reset(); }

	virtual void FrameUpdatePostEntityThink()
	{
		for ( int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer || !pPlayer->IsConnected() )
			{
				m_hPlayer[i - 1] = NULL;
				continue;
			}
			if ( m_hPlayer[i - 1].Get() != pPlayer )
			{
				m_hPlayer[i - 1] = pPlayer;
				m_nSent[i - 1] = 0;
			}

			// a few per frame so the reliable stream never overflows
			CSingleUserRecipientFilter filter( pPlayer );
			filter.MakeReliable();
			for ( int n = 0; n < 24 && m_nSent[i - 1] < s_TerrainMods.Count(); n++ )
			{
				const StoredTerrainMod_t &m = s_TerrainMods[m_nSent[i - 1]++];
				UserMessageBegin( filter, "TerrainMod" );
					EncodeTerrainMod( m.type, m.vCenter, m.vNormal, m.usRadius, m.vecMin, m.vecMax, m.flStrength, m.ucFlags );
				MessageEnd();
			}
		}
	}

private:
	void Reset()
	{
		for ( int i = 0; i < MAX_PLAYERS; i++ )
		{
			m_hPlayer[i] = NULL;
			m_nSent[i] = 0;
		}
	}

	CHandle<CBasePlayer> m_hPlayer[MAX_PLAYERS];
	int m_nSent[MAX_PLAYERS];
};
static CTerrainModSender s_TerrainModSender;

//-----------------------------------------------------------------------------
// Save/restore: a loaded game gets its craters back (collision here, rendering
// on the client through CTerrainModSender).
//-----------------------------------------------------------------------------
static short TERRAINMOD_SAVE_RESTORE_VERSION = 1;

class CTerrainModSaveRestoreBlockHandler : public CDefSaveRestoreBlockHandler
{
public:
	const char *GetBlockName() { return "GammaTerrainMods"; }

	void WriteSaveHeaders( ISave *pSave ) { pSave->WriteShort( &TERRAINMOD_SAVE_RESTORE_VERSION ); }

	void ReadRestoreHeaders( IRestore *pRestore )
	{
		short version;
		pRestore->ReadShort( &version );
		m_bDoLoad = ( version == TERRAINMOD_SAVE_RESTORE_VERSION );
	}

	void Save( ISave *pSave )
	{
		int nCount = s_TerrainMods.Count();
		pSave->WriteInt( &nCount );
		if ( nCount )
			pSave->WriteData( (const char *)s_TerrainMods.Base(), nCount * sizeof( StoredTerrainMod_t ) );
	}

	void Restore( IRestore *pRestore, bool createPlayers )
	{
		if ( !m_bDoLoad )
			return;
		int nCount = pRestore->ReadInt();
		if ( nCount <= 0 || nCount > 1000000 )
			return;
		int nFirst = s_TerrainMods.AddMultipleToTail( nCount );
		pRestore->ReadData( (char *)( s_TerrainMods.Base() + nFirst ), nCount * sizeof( StoredTerrainMod_t ), 0 );
		if ( !GammaTerrainMod() )
			return;
		for ( int i = nFirst; i < s_TerrainMods.Count(); i++ )
		{
			const StoredTerrainMod_t &m = s_TerrainMods[i];
			CTerrainModParams params;
			params.m_vCenter = m.vCenter;
			params.m_vNormal = m.vNormal;
			params.m_vecMin = m.vecMin;
			params.m_vecMax = m.vecMax;
			params.m_flRadius = RemapVal( m.usRadius, 0, 65535, MIN_TMOD_RADIUS, MAX_TMOD_RADIUS );
			params.m_flStrength = m.flStrength;
			params.m_Flags = m.ucFlags;
			GammaTerrainMod()->ApplyTerrainModServer( (TerrainModType)m.type, params );
		}
	}

private:
	bool m_bDoLoad;
};

static CTerrainModSaveRestoreBlockHandler s_TerrainModSaveRestoreBlockHandler;

ISaveRestoreBlockHandler *GetTerrainModSaveRestoreBlockHandler()
{
	return &s_TerrainModSaveRestoreBlockHandler;
}

void TerrainMod_Apply( TerrainModType type, const CTerrainModParams &params )
{
	unsigned short usRadius = (unsigned short)RemapVal( params.m_flRadius, MIN_TMOD_RADIUS, MAX_TMOD_RADIUS, 0, 65535.9f );
	unsigned char ucFlags = (unsigned char)params.m_Flags;

	// Queued for every client (see CTerrainModSender).
	StoredTerrainMod_t &stored = s_TerrainMods[s_TerrainMods.AddToTail()];
	stored.type = (unsigned char)type;
	stored.vCenter = params.m_vCenter;
	stored.vNormal = params.m_vNormal;
	stored.vecMin = params.m_vecMin;
	stored.vecMax = params.m_vecMax;
	stored.usRadius = usRadius;
	stored.flStrength = params.m_flStrength;
	stored.ucFlags = ucFlags;

	// Make sure we have the exact same values as the client does.
	CTerrainModParams unpackedParams;
	unpackedParams.m_vCenter = params.m_vCenter;
	unpackedParams.m_flRadius = RemapVal( usRadius, 0, 65535, MIN_TMOD_RADIUS, MAX_TMOD_RADIUS );
	unpackedParams.m_vecMin = params.m_vecMin;
	unpackedParams.m_vecMax = params.m_vecMax;
	unpackedParams.m_flStrength = params.m_flStrength;
	unpackedParams.m_Flags = ucFlags;
	unpackedParams.m_vNormal = params.m_vNormal;

	// Move players out of the way so they don't get stuck in the terrain.
	float playerStartHeights[MAX_PLAYERS];
	int nPlayers = MIN( MAX_PLAYERS, gpGlobals->maxClients );
	for ( int i = 0; i < nPlayers; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i + 1 );
		if ( !pPlayer )
			continue;

		playerStartHeights[i] = pPlayer->GetAbsOrigin().z;
		trace_t trace;
		UTIL_TraceEntity( pPlayer, pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin() + Vector( 0, 0, unpackedParams.m_flRadius * 2 ), MASK_SOLID, &trace );
		pPlayer->SetLocalOrigin( trace.endpos );
	}

	if ( GammaTerrainMod() )
		GammaTerrainMod()->ApplyTerrainModServer( type, unpackedParams );

	// Move players back down.
	for ( int i = 0; i < nPlayers; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i + 1 );
		if ( !pPlayer )
			continue;

		trace_t trace;
		UTIL_TraceEntity( pPlayer, pPlayer->GetAbsOrigin(),
			Vector( pPlayer->GetAbsOrigin().x, pPlayer->GetAbsOrigin().y, playerStartHeights[i] ), MASK_SOLID, &trace );
		pPlayer->SetLocalOrigin( trace.endpos );
	}
}

// In 2003 TerrainMod_Add and TerrainMod_Apply were identical copies.
void TerrainMod_Add( TerrainModType type, const CTerrainModParams &params )
{
	TerrainMod_Apply( type, params );
}
