# 2003/2018 animation composition audit

Live source: GammaSource/public/bone_setup.cpp. The converter is owned separately; these changes depend on cache v5 marking converted animation and sequence descriptors with STUDIO_GAMMA_LEGACY2003 (0x40000000), defined in public/studio.h. No global animation convar was changed, and no Hammer/weapon/UI source was edited.

## Measured difference: 2D pose blending

The 2003 CalcPoseSingle always blends four grid corners bilinearly with normalized QuaternionBlend. The 2018 implementation defaults anim_3wayblend to 1 and chooses three corners on alternating triangles. This is a change to authored pose interpolation even when every input animation frame is correct.

Original Alyx.mdl sequence191 layer_Aim_all is a 3x4 grid of twelve one-frame delta poses. Sampling 25 interior coordinate pairs per grid cell found 1,979 changed positive-weight bone poses. The maximum local rotation difference is 11.287311 degrees at ValveBiped.Bip01_L_UpperArm with authored bone weight 1.0; L_Hand differs up to 9.817 degrees. Maximum local position difference is 0.201602 units. These comparisons use the original MDL only, so conversion quantization and temporal interpolation cannot explain the difference.

The public runtime now takes its existing four-corner interpolation branch for marked converted sequences. Modern assets retain their previous convar-controlled behavior. Kleiner has no 2D grids, so this change is not claimed as a fix for Kleiner's general posture problems.

Reproduce from the GammaSource directory with Python 3:
```text
tools/gamma/audit_legacy_composition.py <original-Alyx.mdl> <original-Kleiner.mdl> --out <report.json>
```
The actual original paths and sampled data are in animation_composition_audit.json. This tool reuses the existing validated original-frame decoder and reads reference assets without modification.

## Measured difference: wrapped IK error timing

The original Studio_IKAnimationError samples floor((numframes-1)*cycle), retaining its fractional part, then adds numframes for a wrapped rule. The modern weight/sampling function first wraps cycle by 1, equivalent to adding numframes-1. The converter/IK audit found 53 actual wrapped rules; Combine_Strider animation 6 has 91 frames and multiple such envelopes.

Marked converted animations now use the original absolute frame/fraction calculation while the rule envelope is active (weight>=0.0001). Envelope weights still use the existing modern function, whose spline formula matches 2003. Inactive ground-rule lookups retain modern clamping to avoid reproducing old out-of-range reads. Other modern safeguards and the compressed-error fallback remain present. The converter owner is preserving the uncompressed original error records and validating their active sample ranges.

## Differences examined that do not justify extra changes

- DELTA/POST quaternion multiplication order and positional accumulation match 2003: pre-delta uses QuaternionSM, post-delta uses QuaternionMA, and positions add weight*delta.
- Weighted SlerpBones and normalized BlendBones formulas match for ordinary local bones. Neither Alyx nor Kleiner has fractional positive per-bone weights on non-delta base sequences.
- The ALLZEROS optimization uses QuaternionIdentityBlend, a normalized linear identity blend equivalent to the original normalized corner blend; it does not use angular QuaternionScale. No speculative replacement was made.
- The converter already maps original autolayer WEIGHT 0x40 and SPLINE 0x80 into modern ramp endpoints and AL_SPLINE 0x40, preventing accidental AL_XFADE 0x80. POST 0x10 is preserved. Original layer ramps are therefore not blindly reinterpreted.
- Neither inspected model has original sequence flag 0x80, so a sequence CYCLEPOSE collision is not evidenced in these scenes.
- The separate IK audit found missing original animation rules and sequence locks (44 Alyx locked sequences, 35 Kleiner), providing a concrete explanation for unstable extremities beyond raw pose data. That metadata repair belongs to the converter owner.

Backups: backups/animation_composition. Full byte-preserving source diff and SHA-256 manifest: animation_composition.patch and animation_composition_changes.json. No build or game launch was performed here. Combined cache-v5 build/runtime validation is pending with the coordinating agent; these measurements are not a claim that both scene issues have passed in game.
