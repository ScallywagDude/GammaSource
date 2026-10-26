# 2003 blood and impact restoration - source review and runtime probes

The code uses the original TE_BloodSprite + FX_Blood path for Gamma. The original damage/10 size clamp (3..16), RGB (64,0,0 singleplayer red; 255,32,32 multiplayer red; 128,128,0 yellow), alpha 255, and preexisting .25s sprite animation are retained. Mechanical damage still sparks/smokes. The normal server TraceBleed projection supplies persistent wall blood using the restored Blood / YellowBlood groups.

Private materials are selected only when their VMT exists. Missing private assets fall back to the mounted material name. Client and server both select scripts/gamma_2003_decals.txt when installed; otherwise they use the unchanged decals_subrect baseline. The private script preserves modern groups beyond the original 2003 impact groups.

No surface response, damage, weapon values, normal-map shaders, ground deformation, or native decal projection code was replaced. gamma_impact_debug reports the actual bullet decal group/material chosen by that projection path. gamma_impact_probe reports actual hit normal, surface name, game material, friction/density and surface shader/base/bump textures. These remain diagnostic data, not substitutes for visible QA.

Copy gamma_impact_checks.cfg to an isolated test mod's cfg directory, aim at a normal nonsky wall, and exec gamma_impact_checks. Duration 8 seconds. Do not run this in the delivered user profile. Expected: GAMMA_IMPACT_CHECK_COMPLETE, all private material error=0, positive blood sprite IDs, successful native impact hit, red/yellow blood local counts +2, and r_printdecalinfo dynamic count increasing then persisting at 8 seconds. Different underlying surfaces and overlap can produce different decal-count increments. Examine the captured JPEGs.

For actual weapon/NPC validation: enable gamma_impact_debug 1 and gamma_blood_debug 1; record gamma_blood_info; shoot a bleeding, damageable NPC with a beta weapon, then record gamma_blood_info again. A positive NETWORK count delta (distinct from local probe effects), RGB/size log and nonzero spray ID verify the original server-to-client path. Put the NPC in front of a nearby wall and verify Blood decals persist. Check red and yellow creatures; mech targets should still spark. Fire at concrete/metal/wood/glass and inspect corresponding private Impact groups. No command here asserts actual damage or visual success automatically.

Build/runtime results: PENDING, owned by parent agent. No build or game launch was performed by this source agent.
