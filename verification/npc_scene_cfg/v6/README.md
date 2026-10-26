# Fixed-frame native intro and client pose capture

Private CFGs live in t_round3/cfg, with source copies here. Both use the original e3_lab map and unchanged VCD, camera observer distance100, host_framerate60, native scene tracing, snd_showstart1, and gamma_legacy_pose_debug1. They preserve player placement.

1. Run `+exec gamma_v5_poseonly_boot.cfg` with the new diagnostic client but existing v5 datacache. This records 54 client Alyx snapshots at0.5-second intervals and three Kleiner snapshots. No new TGA movie is created. This intentional mixed validation state is distinct from the previously recorded v5 raw movie.
2. Install the full v6 DLL set, then run `+exec gamma_v6_every_frame_boot.cfg`. It records 27.2seconds as `gamma_v6_intro0000.tga` onward, expected1632 consecutiveframes at60fps. The same54+3 client snapshots accompany it.

Alyx is entity167 and Kleiner168 in this original map. `gamma_actorinfo` at capture start logs this assumption; reject a run with different actor IDs. Start/end native scene info, actor state, and player position are recorded. The snapshot command reports actual client time/frame values, so use those rather than assuming gamma_delay is exact to one render frame.

After moving only the completed movie into its dedicated proof directory, run using the bundled Pillow/NumPy Python:

```
python tools/gamma/audit_capture_frames.py <movie-folder> --prefix gamma_v6_intro --out <proof>/v6_every_frame_audit.json
```

The audit decodes and SHA256-hashes every TGA. It checks count/continuity/dimensions and writes per-frame CSV metrics for body, torso/hands, legs, a head control, and static background. It ranks local second-difference motion peaks for visual review. It does not certify animations by a motion threshold. Original TGAs are read without copying or modifying them.

The preserved v5 baseline audit found exactly1632 frames0000..1631,1024x640,24-bituncompressedTGA, total3,208,671,936bytes. No gaps, duplicates, format changes, or decode errors occurred. The background control pixels were identical across every frame, confirming a static view. Strongest body discontinuity was frame759(12.65srelative) with local robustz45.74 and second-difference mean9.246/255. Another large candidate was frame1550(25.833s). Valid authored movement can also produce peaks; inspect before/after source fixes with client bone/procedure data.
