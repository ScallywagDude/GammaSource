//========= GammaSource ============//
//
// Client stubs for the 2003 weapons and entities that only need their
// networked base class on the client (the 2003 cl_dll had the same stubs).
//
//=================================//

#include "cbase.h"
#include "c_weapon__stubs.h"
#include "basehlcombatweapon_shared.h"
#include "c_basehlcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

STUB_WEAPON_CLASS( weapon_ar1, WeaponAR1, C_HLMachineGun );
// the 2003 AR2; the 2018 C_WeaponAR2 stub is now weapon_ar2future
STUB_WEAPON_CLASS( weapon_ar2, WeaponAR2Beta, C_HLMachineGun );
STUB_WEAPON_CLASS( weapon_brickbat, WeaponBrickbat, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_cguard, WeaponCGuard, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_extinguisher, WeaponExtinguisher, C_HLSelectFireMachineGun );
STUB_WEAPON_CLASS( weapon_hmg1, WeaponHMG1, C_HLSelectFireMachineGun );
STUB_WEAPON_CLASS( weapon_hopwire, WeaponHopWire, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_iceaxe, WeaponIceAxe, C_BaseHLBludgeonWeapon );
STUB_WEAPON_CLASS( weapon_immolator, WeaponImmolator, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_irifle, WeaponIRifle, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_manhack, Weapon_Manhack, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_molotov, WeaponMolotov, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_smg2, WeaponSMG2, C_HLSelectFireMachineGun );
STUB_WEAPON_CLASS( weapon_rollerwand, WeaponRollerWand, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_sniperrifle, WeaponSniperRifle, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_thumper, WeaponThumper, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_tripwire, Weapon_Tripwire, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( player_manhack, Player_Manhack, C_BaseCombatCharacter );
STUB_WEAPON_CLASS( weapon_vortexhopwire, WeaponVortexHopwire, C_BaseHLCombatWeapon );	// HL2: Aftermath vortex hopwire
STUB_WEAPON_CLASS( weapon_proto1, WeaponProto1, C_BaseHLCombatWeapon );	// Episode Three prototype (HL2: Aftermath)
