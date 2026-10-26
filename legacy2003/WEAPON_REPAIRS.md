# GammaSource weapon repairs — 2026-10-25

Launch `GammaSource/play.cmd`. This is the tested 64-bit build; the launcher supports running alongside the original 2003 game.

The 2003 hands are mandatory defaults. Native beta viewmodels and their hand material/texture library match the original 2003 build. Added retail viewmodels use private aliases with their retail arms hidden and original beta donor geometry. The crossbow is included. The legacy `cl_betahands 0` setting clamps to 1. Original arm-free utility models retain their 2003 geometry.

Repairs cover beta ammo limits and damage, ammo pickups, the complete impulse 101 inventory, firing and burst cooldowns, reload/throw activity compatibility, Molotov/flare/stunstick availability, selector icons and scrolling, walking/jumping surface multipliers, and remote manhack controls. The vortex hopwire now automatically starts its circular black-hole effect, pulls and consumes physics objects, and cleans up. The Alyx gun uses its 2018 implementation with the 2003 model and draw/fire/reload animations.

Validation: a 42-weapon roster exercise followed by focused regression retests; final 43-weapon inventory with zero invalid ammo limits; Alyx firing five rounds in 0.5 seconds and reloading in 2.533 seconds; walking/jumping on all four E3 maps; manhack flight and three exit paths; vortex throw/lob/roll/trap and a default-setting black-hole pull/consumption test; 43-weapon hands provenance audit and visual/map-change checks. JPEG screenshots were repaired and verified as well.

Sources were edited in the existing GammaSource-main and source-engine-master trees. The source ZIP contains changed engine/game sources and tooling plus the complete 2018 content overlay; merge it over the supplied handoff trees. Large original game-content layers remain required. `apply_weapon_repairs.py` covers the core ammo/movement subset; the source ZIP includes the other repairs too. Backups and detailed test evidence remain beside this document in the repair artifact folder.
