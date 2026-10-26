# v28 known bugs and remaining work

This is the frozen build's disclosure list, not a claim that all NPC work is finished.

## Confirmed defects / incomplete features

- **Miniturret fallback aiming:** the beta Floor Sentry art points its muzzle along local -X, while the fallback yaw calculation uses the entity's +X orientation. The native Miniturret probe acquired its enemy but inflicted no damage. Sentry shares this fallback and still needs its own corrected aiming test. Preferred `miniturret.mdl` / `sentry.mdl` art is absent from the supplied assets.
- **Missing Alyx story clips:** `pos07_pos02` and `actionturn01` are absent from all inspected supplied Alyx variants. Four older, narrow posture fallbacks remain; exact original transitions cannot be promised.
- **Alien Assassin is a reconstruction:** shock tripwires, pests and rope running are not implemented. Its thrown blade uses a sprite trail because no standalone knife mesh was supplied. Historical `npc_assassin` still selects the Combine/Female Assassin; use `npc_alienassassin` for the reconstructed alien.
- **Cremator is a reconstruction:** original AI and balance are unavailable. Cleanup cannot discover purely client-side ragdolls. Full patrol/corpse-route coverage and the final head physics compatibility change still need native verification.
- **TF2 bug prototypes are not integrated:** Builder, Warrior and Bughole models are missing; their camouflage, buildable-object and client-table dependencies remain unresolved. Historical HL1 monster integration into HL2 mode is also unfinished.
- **Model coverage has limits:** 22 supplied v37 variants have pre-existing unsupported/truncated/external-animation layouts. Some originals still contain missing or malformed material references, including stray `// phy` paths. Existing Combot prop physics and several optional map resources still warn.

## Verification gaps

- Conscript, Sentry, Combine Assassin, Combot, Combine Guard/Elite, Crab/Mortar Synth, rollers, live Hydra attacks and Spotlight require a broader per-class gameplay matrix. Enabled classes and build registration do not prove every behavior works.
- Physgun basic grab/lift/release and beam are verified. Standalone pellet welding and held-object save/load are not certified; the short reload/holster probe is not a full welding test.
- Broad NPC navigation, squad tactics, grenades, deaths, ragdolls, interruption, multiplayer and long-session stability have not all been exercised. The lab is a limited test arena.
- Targeted HEV/skybox coverage, friendly-fire toggles and all renderer/UI regressions have not been repeated on this final snapshot. Prior fixes are preserved.
- Old map sound entries such as `eli_lab.eli_labcoat`, `Hud.Hint` and optional later Strider/Scanner sounds remain missing in supplied scripts. Debug shaders and optional VGUI materials can also warn. Original tested lab/Hydra dialogue WAVs are present.

## Previously queued work

- Remove every Steam dependency; staged local content does not mean all Steam-related code/dependencies have been removed.
- Combine Hammer++ features with the exact 2003 WorldCraft appearance. This build enables normal Hammer; it does not implement that replacement.
- Repair .357/crossbow hand stretching, the crossbow logo, all added-weapon ammo availability/pickups and `weapon_proto1`'s .357 ammo models, as recorded in `TODO.md`. Existing weapon balance/audit work is preserved.
