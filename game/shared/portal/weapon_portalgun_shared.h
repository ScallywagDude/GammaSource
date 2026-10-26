//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_PORTALGUN_SHARED_H
#define WEAPON_PORTALGUN_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

#ifdef CLIENT_DLL
#include "c_weapon_portalgun.h"
#else
#include "weapon_portalgun.h"
#endif

#define PORTALGUN_BEAM_SPRITE "sprites/grav_beam.vmt"
#define PORTALGUN_BEAM_SPRITE_NOZ "sprites/grav_beam_noz.vmt"
// GammaSource: Portal's additive versions, under their own folder; HL2 and the beta content have
// "Sprite"-shader materials with the same names, which draw as black squares here
#define PORTALGUN_GLOW_SPRITE "sprites/portalgun_fx/glow04_noz"
#define PORTALGUN_ENDCAP_SPRITE "sprites/portalgun_fx/grav_flare"
#define PORTALGUN_GRAV_ACTIVE_GLOW "sprites/portalgun_fx/grav_light"
#define PORTALGUN_PORTAL1_FIRED_LAST_GLOW "sprites/portalgun_fx/bluelight"
#define PORTALGUN_PORTAL2_FIRED_LAST_GLOW "sprites/portalgun_fx/orangelight"
#define PORTALGUN_PORTAL_MUZZLE_GLOW_SPRITE "sprites/portalgun_fx/portalgun_effects"
#define PORTALGUN_PORTAL_TUBE_BEAM_SPRITE "sprites/portalgun_fx/portalgun_effects"

#define	PORTALGUN_FASTEST_REFIRE_TIME		0.1f
#define	PORTALGUN_FASTEST_DRY_REFIRE_TIME	0.2f

enum
{
	EFFECT_NONE,
	EFFECT_READY,
	EFFECT_HOLDING,
};

extern ConVar sk_auto_reload_time;

#endif // WEAPON_PORTALGUN_SHARED_H
