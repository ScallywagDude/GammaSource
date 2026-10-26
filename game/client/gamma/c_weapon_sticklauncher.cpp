//========= GammaSource: ported from the 2003 code (cl_dll/hl2_hud/c_weapon_sticklauncher.cpp) ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "basehlcombatweapon_shared.h"

#include "c_weapon__stubs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

STUB_WEAPON_CLASS( weapon_stickylauncher, WeaponStickyLauncher, C_BaseHLCombatWeapon );

class C_StickyBomb : public C_BaseAnimating
{
	DECLARE_CLASS( C_StickyBomb, C_BaseAnimating );
public:
	C_StickyBomb() {}


	DECLARE_CLIENTCLASS();

private:
	int m_boneIndexAttached;
	Vector m_bonePosition;
	QAngle m_boneAngles;
};

IMPLEMENT_CLIENTCLASS_DT( C_StickyBomb, DT_StickyBomb, CStickyBomb)
	RecvPropInt( RECVINFO(m_boneIndexAttached) ),
	RecvPropVector( RECVINFO(m_bonePosition) ),
	RecvPropFloat(RECVINFO(m_boneAngles[0]) ),
	RecvPropFloat(RECVINFO(m_boneAngles[1]) ),
	RecvPropFloat(RECVINFO(m_boneAngles[2]) ),
END_RECV_TABLE()
