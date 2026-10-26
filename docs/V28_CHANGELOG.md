# v28 changelog — 26 October 2026

Development stopped when the account reached 90% usage, as requested. This release records completed work and remaining limitations separately. NPC restoration is not complete.

## Models, animation and scenes

- Preserve original procedural axis/quaternion helper bones, their control-bone dependencies, and original IK rules and locks during v36/v37 conversion. Normalize helper quaternions in release builds.
- Correct additive animation origins and eye/eyelid structure fields. Preserve original sequence KeyValues, including Faceposer posture/gesture classifications. Regenerate incompatible conversions in `legacy_models_cache_v7`.
- Apply original four-corner pose blending and wrapped IK sampling to converted 2003 assets. Retain modern behavior for other models.
- Restore original gesture behavior while respecting authored posture metadata. Add normal-scene event tracing and an observer camera that does not move the player through map triggers.
- Prevent pending-delete Strider calibration helpers from recursively exhausting the entity pool after a model failure.

## Physgun and effects

- Replicate beam endpoints through explicit networked fields; retain the original blue Bezier beam and beta viewmodel.
- Correct pickup callback order, motion-controller cleanup, dropped-object handling and physgun-owned movement overrides.
- Restore original blood sprites/spray and persistent surface impact decals with a private, reproducible asset overlay.
- Repair reflective puddle materials and legacy six-face cubemap loading; preserve the original masks and reflections.

## NPCs

- Enable and adapt original Conscript, Miniturret and Sentry implementations without duplicating the existing ceiling-turret registration.
- Restore 112 original Bullsquid/Houndeye relationship pairs and nonzero historical skill defaults. Repair beta spit, tailwhip, sonic-wave and animation-event paths.
- Repair Combine Assassin numeric animation events, Hydra impale/resource safety and Spotlight state/effect cleanup.
- Port original Combot inspection, flight, gas and dive behavior. `npc_cscanner`/`npc_combot` use the beta implementation; the later implementation remains `npc_cityscanner`.
- Repair Mortar Synth health and energy-projectile damage attribution, visibility and lifetime handling.
- Add an explicitly reconstructed Cremator using original models, sounds and activities, with beam damage, corpse cleanup, save/restore and death parts.
- Add a separate, explicitly reconstructed Alien Assassin using the original model, knife/claw events and smoke escape. Unsupported planned abilities are listed in the known bugs.
- Supply missing original spitball and Combine Elite assets with provenance and dependency checks.

## Build and delivery

- Build normal Hammer by default with the Windows x64 targets; adapt MFC, pointer-width callbacks and editor dependencies. Hammer++/WorldCraft replacement remains queued.
- Stage the unified source with an explicit original-content directory and preserve the GitHub repository's existing root layout.
- Preserve prior weapon values, mandatory 2003 hands, HUD/options work and the classic loading dialog on plain black.

## Recorded validation

- Final native Windows x64 compile and install passed, including normal Hammer and the Hydra client renderer. All 16 targeted regression tests passed. Final logs and matching installed/staged binary hashes accompany the build.
- Actual 60 FPS captures contain 1,632 baseline and 1,633 corrected consecutive lossless frames. Live procedural helper transforms match original calculations within 0.00014 units and 0.00006 degrees in sampled Alyx/Kleiner poses.
- Native v7 walking tests preserve three original Alyx metadata payloads; posture weight fades during movement and the collapsed-leg regression is absent in inspected captures.
- Physgun pickup/lift/release and visible beam passed with an original 250 kg prop and an independently extracted modern 30 kg prop.
- Bullsquid and Houndeye inflicted real damage and resumed after save/load. Cremator and Alien Assassin also inflicted real damage and resumed after restore.
- Registration counts and successful compilation are not certification of every NPC, map, attack or visual effect. See [known bugs](V28_KNOWN_BUGS.md).
