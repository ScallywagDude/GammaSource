#ifndef GAMMA_PLAYER_CHARACTERS_H
#define GAMMA_PLAYER_CHARACTERS_H
#pragma once

struct GammaPlayerCharacter
{
	const char *alias;
	const char *name;
	const char *model;
};

static const GammaPlayerCharacter g_GammaPlayerCharacters[] =
{
	{ "gordon", "Gordon Freeman (Survivor)", "models/gamma/player/gordon.mdl" },
	{ "chell", "Chell (Portal)", "models/gamma/player/chell.mdl" },
	{ "chell_2010", "chell_2010", "models/gamma/player/chell_2010.mdl" },
	{ "mel", "mel", "models/gamma/player/mel.mdl" },
	{ "gasmask_citizen", "Gasmask Citizen", "models/gamma/player/gasmask_citizen.mdl" },
	{ "hl2dm_rebel", "HL2DM Rebel", "models/gamma/player/hl2dm_rebel.mdl" },
	{ "hl2dm_female", "HL2DM Female Rebel", "models/gamma/player/hl2dm_female.mdl" },
	{ "hl2dm_combine", "HL2DM Combine Soldier", "models/gamma/player/hl2dm_combine.mdl" },
	{ "css_urban", "CSS Urban", "models/gamma/player/css_urban.mdl" },
	{ "css_gign", "CSS GIGN", "models/gamma/player/css_gign.mdl" },
	{ "css_gsg9", "CSS GSG9", "models/gamma/player/css_gsg9.mdl" },
	{ "css_sas", "CSS SAS", "models/gamma/player/css_sas.mdl" },
	{ "css_phoenix", "CSS Phoenix", "models/gamma/player/css_phoenix.mdl" },
	{ "css_leet", "CSS Leet", "models/gamma/player/css_leet.mdl" },
	{ "css_guerilla", "CSS Guerilla", "models/gamma/player/css_guerilla.mdl" },
	{ "css_arctic", "CSS Arctic", "models/gamma/player/css_arctic.mdl" },
};

#endif
