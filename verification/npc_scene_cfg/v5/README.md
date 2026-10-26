# Cache-v5 native scene validation

Use only the isolated `source2018_install/t_round3` profile. The test boots intentionally clear its bindings. Production game profiles are unchanged. No OS input or foreground activation is needed.

Launch separately, audio enabled, with the coordinating agent's hidden background launcher pattern:

- `+exec gamma_v5_lab_observer_boot.cfg`: fresh map, original automatic acting_intro and natural sequence2 chain; 70 seconds. Observer camera does not move the player. Captures Alyx, Kleiner, eyes, and before/after scene states. Sequence3 must remain unstarted because the real player never crosses its trigger.
- `+exec gamma_v5_hydra_observer_boot.cfg`: normal original uhoh_lcs Start input; 25 seconds. Captures all four original monster_generic sewer puppets and their authored e3_sewer cycles, plus Alyx before/during/after. No instanced scene injection.
- `+exec gamma_v5_gesture_walk_boot.cfg`: controlled regression, 40 seconds. Deliberately cancels native scenes, then drives normal logic_choreographed_scene probes. Goal A/B are placed at exact original e3_lab info_node 2/8 coordinates. Interrupts one legacy gesture walk, restarts, repeats to the opposite goal, then repeats an explicit posture control. Original story files remain unchanged. If route formation fails, the moving-sample checks fail rather than certifying an idle test.

`snd_showstart 1` is enabled in all three boots. It prints engine DynamicSound/StaticSound starts after successful audio mixer creation. Background muting can still prevent listening, and traced starts do not prove audible mix quality.

Run from GammaSource:

```
py -3 tools/gamma/check_scene_runtime.py <captured-log> --kind lab --out <report.json>
py -3 tools/gamma/check_scene_runtime.py <captured-log> --kind hydra --out <report.json>
py -3 tools/gamma/check_scene_runtime.py <captured-log> --kind walk --out <report.json>
```

The checker records explicit pass/fail checks, warning lines, engine waveform starts, actor movement extent, and event finishes. It requires native scenes to finish uncanceled and actors to release. Walk checks require actual moving samples: the original gesture stays weighted during motion, explicit faceposer/type=posture suppresses its overlay, and cancel/restart/repeat completes. Rendered poses/skinning must still be inspected in screenshots.

## Source and sound evidence

The current marker-gated gesture default matches 2003 AddFlexGesture, which applies event intensity regardless of movement. Modern unmarked defaults and explicit faceposer metadata remain in effect. No new C++ changes were needed in this validation task.

`v5_scene_sound_asset_audit.json` audits all 29 SPEAK events across acting_intro, sequence2, sequence3, and uhoh. All 29 resolve to exact original script token streams and byte-identical original WAVs. It records source paths, hashes, RIFF format, and duration. No waveforms or script entries were fabricated.

Five unrelated existing precache warnings have no definitions in the mounted scripts or any supplied original hl2 script: eli_lab.eli_labcoat, Hud.Hint, NPC_CScanner.DeployMine, NPC_Strider.StriderBusterExplode, and NPC_Strider.Hunt. The scanner script contains only the distinct NPC_SScanner.DeployMine entry. The original response_rules references eli_lab.eli_labcoat but supplies neither its definition nor a labcoat-named WAV. These remain visible limitations. Four Portal manifest files absent from physical roots may resolve from mounted VPKs; the physical-only sound asset audit does not certify those packages.

The previous observer log independently showed unmodified intro/sequence2 completion at 27.075/35.970 seconds, unchanged player origin, and no sequence3 start. Those earlier snapshots are not cache-v5 runtime proof. The two missing Alyx animation clips pos07_pos02/actionturn01 remain missing source assets; no unrelated clip is substituted.
