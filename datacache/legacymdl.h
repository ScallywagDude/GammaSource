//========= GammaSource ============//
//
// Load-time conversion of MDL v36/v37 (2001-2003) models to MDL v48 + .vvd + VTX7.
//
//=================================//

#ifndef LEGACYMDL_H
#define LEGACYMDL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"

struct LegacyVtxInput_t
{
	const char *pSuffix;	// ".dx80.vtx", ".dx90.vtx", ".sw.vtx", ...
	const void *pData;
	int nSize;
};

struct LegacyVtxOutput_t
{
	char suffix[32];
	CUtlBuffer data;
	LegacyVtxOutput_t() {}
	LegacyVtxOutput_t( const LegacyVtxOutput_t &o ) { V_strncpy( suffix, o.suffix, sizeof( suffix ) ); data.Put( o.data.Base(), o.data.TellMaxPut() ); }
	LegacyVtxOutput_t &operator=( const LegacyVtxOutput_t &o ) { V_strncpy( suffix, o.suffix, sizeof( suffix ) ); data.Purge(); data.Put( o.data.Base(), o.data.TellMaxPut() ); return *this; }
};

// True if the blob is an IDST v36 or v37 studio model.
bool LegacyMDL_IsLegacy( const void *pData, int nSize );

// Converts a v36/v37 model and its VTX6 files. 'report' receives what was dropped (or why it failed).
bool LegacyMDL_Convert( const void *pMdl, int nMdlSize, const LegacyVtxInput_t *pVtx, int nVtx,
	CUtlBuffer &mdlOut, CUtlBuffer &vvdOut, CUtlVector<LegacyVtxOutput_t> &vtxOut, CUtlString &report );

#endif // LEGACYMDL_H
