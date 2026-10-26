# Beta Miniturret and Sentry port

Source: the dormant original 2003 family in `game/server/hl2/npc_turret.cpp`, compared with the user's Mega Build `modernization_v18/dlls/hl2_dll/npc_turret.cpp`.

The active classes are `npc_miniturret` and `npc_sentry`. Their shared code retains the original manual deploy/search/aim/fire/retire state machine, automatic activation spawn flags, armor-hit resistance, inactive damage resistance, low-health berserk behavior and animated smoke/spark deaths. Mini uses Pistol ammunition; Sentry uses SMG1, activates on touch/damage, has the original taller collision envelope and long search timeout. These are separate behaviors even when the available fallback art is shared.

The original source's dormant ceiling template is disabled. The existing genuine 2003 `CNPC_CeilingTurret` in `npc_turret_ceiling.cpp` remains the sole active `npc_turret_ceiling` registration. Its `Combine_turrets/Ceiling_turret.mdl` has `ACT_CEILING_TURRET_*`, unlike the shared family's `ACT_TURRET_*` names. Do not replace that model with an incompatible activity set or add another map-facing classname just to avoid a linker collision.

Port repairs include current think/touch and damage signatures, private-field accessors, valid collision types, activity registration before model activity indexing, nonzero health defaults, explicit model-key preservation, safe muzzle attachment lookup, sprite cleanup, saved sound-selection state, Enable/Disable inputs and death outputs. Defaults of 10 health for Mini and Sentry come from the supplied original `hl2/cfg/skill2.cfg` and `skill3.cfg`.

## Asset limitations and explicit fallbacks

The preferred historical `models/miniturret.mdl` and `models/sentry.mdl` are absent from the supplied Mega Build and mounted original content. The code uses either preferred file if supplied, and preserves explicit map `model` values. Otherwise it uses the actual provided 2003 `models/Sentry_Guns/Floor_Sentry.mdl`; this is an explicit fallback, not recovered unique Mini/Sentry art.

That original beta model has five matching `ACT_TURRET_*` sequences, numbered attachments `0`/`1`, and one `XR` pitch parameter spanning -45 to +45 degrees. The fallback path uses the numbered muzzle, this pitch parameter and base yaw rotation. The original two-parameter aiming path remains available for models with `aim_yaw`/`aim_pitch`.

The older `turret/tu_*.wav` and `weapons/hks1..3.wav` referenced by this dormant source are also absent. When present, their original script sounds are preferred. Otherwise the code uses the supplied original 2003 waves:

- `npc/turret_floor/ping.wav`, `deploy.wav`, `retract.wav`, `alert.wav`, `die.wav`.
- `npc/entity/turret/turret_fire1.wav` for gunfire.

The existing original sound and model files are read in place; no shared assets were overwritten.

## Native verification required

Source presence and build registration do not establish gameplay completion. The coordinating agent must compile this file with the server and verify both classnames spawn with positive health and the selected model, deploy/retire animations advance, an explicitly hostile reachable target takes real bullet damage, yaw/pitch track the target, Enable/Disable/touch activation work, deaths finish, outputs fire once and save/load restores state. Tests should use only the isolated game profile and should also check original sound/material dependencies.

The first compile exposed obsolete direct animation-field access and old collision names. These were replaced with the current public APIs. The next compile/runtime result is recorded by the coordinating agent separately; this document does not claim a pass before that evidence exists.
