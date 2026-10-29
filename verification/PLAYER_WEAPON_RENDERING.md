# Player weapon rendering checks

The local weapon retains the server's view-model model and animation state for
prediction. `CGammaWeaponWorldModelScope` chooses a valid world-model animation
by sequence name during a third-person or portal draw and restores model,
sequence, cycle, and playback rate afterward. PortalBase delegates to this
common path, and portal ghost renderables keep their own model instances.

The hand grip for the original Portal world gun comes from Chell's original
`wrist_R -> weapon_bone` transform. Palm frames defined by index/pinky base
positions normalize Portal's wrist axes to ValveBiped's axes. The barrel's
negative Z axis therefore points along the ValveBiped fingers rather than up.

The immolator's legacy Bone01 bind turns the model-space negative Y barrel
onto bone-local positive Z. A positive 90 degree rotation about the hand's Y
axis maps that barrel onto the hand's forward X axis while retaining its pivot.
This uses the bind matrix and observed native pose, not mesh registration.

The owned HL2DM revolver world model is staged under `models/gamma/weapons/`.
Its native right-hand bone merges with the player rig. Its weapon script
preserves every original byte except the `playermodel` value. The gravity gun
retains the requested 2001-2002 Gamma view and world geometry. Its script changes
only the world-model path to `gamma/weapons/w_early_physgun`. The VVD/VTX
companions are byte-identical to the original gun (which has no PHY). Named attachments added to
its MDL place world effects on the original coil anchor. Its hand axes come from
the original animated wrist/palm frame; native contact checks select the model's
original zero pivot.

The Portal view model retained its hand vertices and animation rig, but Gamma
renamed its hand material to `gamma_hidden_hands`. The overlay restores the
original visible hand binding at the same `v_gamma_portalgun` model path. Its
material's stale texture reference is corrected to the texture actually shipped
beside the Portal gun. `weapon_player_model_manifest.json` records the archive
provenance and hashes in the working asset package.

The Portal view mesh is also compiled with current hardware skinning batches,
preserving all 8495 vertices, 8879 triangles, 45 original bones, and 17 original
attachments. Its ten original animations come from a private animation-only
copy of the owned original MDL, with no fabricated animation timing. The native
idle hand lies below the view-model camera frame, so its stock pose is preserved.

The gravity gun's server-created glows and beams attach to its view model.
`CBaseViewModel::GetAttachment` maps those queries by attachment name to the
scoped world model during third person and portal rendering. Normal first person
retains the original view-model attachments. Missing prototype prong indices
use the actual coil anchor rather than an unrelated view-model origin.

## Automated checks

`player_character_work/tools/test_weapon_worldmodel_scope.cpp` includes the
actual production scope header with test doubles. It checks name-based sequence
translation, idle fallback, full state restoration, and restoration when bone
setup fails. Its compiled test executable returns zero on success.

`stage_weapon_player_models.py` asserts that normalizing the `playermodel`
field makes each staged script byte-identical to its source script.

`test_viewmodel_world_attachments.py` compiles the actual production helpers and
checks first-person isolation, third-person/portal routing, name-based index
translation, prototype coil fallback, and prediction state restoration when
attachment lookup fails.

`test_static_weapon_grips.py` checks production grip matrices against the real
model inverse binds: Portal and immolator barrels point along the hand's forward
axis. `early_physgun_fx_anchors.json` records unchanged mesh companion hashes and
the original coil reference used by the new attachments.

## Native results, 2026-10-28

The final isolated `character_lab` run passed the pistol, revolver, original
early gravity gun, Portal gun, and immolator holding checks. The early gravity
gun's zero-pivot grip contacts the right hand; its orange core glow now sits on
the gun. The immolator uses the heavy physics-gun hold at the waist, with its
orange flame nozzle pointing forward.

All five held weapon models were visible through a linked portal. Returning to
first person preserved their weapon models and animations, without invalid
world-model sequence warnings. The Portal gun's primary and secondary firing
both selected the original `fire1` sequence with advancing cycles (0.120 and
0.147); drawing selected `draw` with cycle 0.060. The idle sequence intentionally
has one frame, so its zero duration does not indicate a failed animation.

Native diagnostics locate the Portal idle right hand at camera-local
`(7.293, -8.182, -11.346)`, below the view-model frame. The mandatory beta-hand
donor maps twenty bones and places its right hand at that same anchor. This
preserves the original Portal pose rather than inventing a visible hand pose.

Evidence: `player_weapons/final_character_native.log` and the isolated runtime's
`weapon_*_grip.jpg`, `portal_weapon_*.jpg`, `firstperson_weapon_*.jpg`, and
`portal_vm_{idle,primary,secondary,draw}.jpg` captures.

The native checks cover these five representative weapon families with Gordon
and the linked-portal test room. They do not claim exhaustive testing of every
weapon on every character, every animation blend, or every portal-straddling
pose. The immolator's original world/view meshes differ; its correction uses
bind axes and native contact checks, not unconstrained shape registration.

## Staged weapon assets

The weapon manifest contains 28 files, 5,294,019 bytes: eleven model/animation
files, fifteen material/texture files, and two weapon scripts. The immolator
port has its own separate manifest. The requested early gravity-gun silhouette
is preserved; the rejected HL2DM retail gravity-gun donor files were removed.

The early gun's exact unchanged companion hashes are:

- VVD: `407213fe8f80117a743d50bbc573187338a9c2c6b47c7a306acb10087e362d07`
- VTX: `16f49351728280a7e43c06b28e500283c53c6c790a3fa281bb45b81c1223dd36`

The original early gun has no PHY companion. Its MDL changes only the internal
alias, attachment table, required attachment bone flag, and file length. The
staging script proves that the weapon script differs only in `playermodel`;
the original first-person model, weapon behavior, sounds, and ammunition remain.

The gravity-gun view and world silhouettes remain the original early Gamma
models; the HL2DM retail gravity gun is not selected by the weapon script.
