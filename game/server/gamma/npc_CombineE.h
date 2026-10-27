//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose:		This is the camo version of the combine soldier
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#ifndef	NPC_COMBINE_E_H
#define	NPC_COMBINE_E_H

// GammaSource: built on the ported 2003 soldier, as in 2003. Its 2003 model
// has the 2003 soldier's animation set, not the HL2 soldier's.
#include "npc_combine2003.h"

//=========================================================
//	>> CNPC_CombineE
//=========================================================
class CNPC_CombineE : public CNPC_Combine2003
{
	DECLARE_CLASS( CNPC_CombineE, CNPC_Combine2003 );

public:
	void		Spawn( void );
	void		Precache( void );
};

#endif	//NPC_COMBINE_E_H
