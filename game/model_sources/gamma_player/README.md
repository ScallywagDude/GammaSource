# Player body authoring sources

`chell/`, `chell_2010/`, `mel/`, and `gasmask_citizen/` contain the complete reference mesh, bind
pose, collision mesh, QC, and import report for the rebuilt player bodies.
The corresponding compiled assets are in `gammasource_overlay/models/gamma/player/`.

Use the owned Portal `studiomdl.exe` with `-game` pointing to a valid compile mod
that can find the overlay's `models/gamma/player/female_anims.mdl`:

```powershell
& $StudioMdlPath -game $CompileGamePath -nop4 .\chell\model.qc
& $StudioMdlPath -game $CompileGamePath -nop4 .\chell_2010\model.qc
& $StudioMdlPath -game $CompileGamePath -nop4 .\mel\model.qc
& $StudioMdlPath -game $CompileGamePath -nop4 .\gasmask_citizen\model.qc
```

After compiling, restore the original eye projection axes using each model's
`eye_report.json` and the portable helper (Python and NumPy are required):

```powershell
python .\tools\player_character_assets\character_eyes.py $CompiledMdlPath $EyeReportPath
```

The QC supplies real eye mesh records, centers, radii, and iris scales. The
post-compile helper re-expresses the authored look vectors in the compiled head
frame, avoiding the compiler's fixed +X assumption for a rig whose bind pose
faces -Y. The bulk rebuild tools apply this step automatically.

The compiler writes the matching MDL, VVD, VTX, and PHY files beneath that game
path. Copy the complete set into the game overlay together. Its shared checksum
must match across all companions. The animation library and materials are
required at runtime; the per-file overlay manifest is under
`verification/player_characters/asset_manifest.json`.

`tools/player_character_assets/retarget_model.py` is a portable conversion tool.
It requires Python and NumPy and accepts these positional arguments:

```text
source.mdl female_anims.mdl output_directory alias material_directory
```

The original model's VVD and DX90 VTX companions must be next to its MDL.
`--eyes-source original.mdl` can supply the original eye records when the input
geometry came from an older conversion that dropped them.
The converter uses anatomical body and palm axes, maps physical joint
directions, preserves ancestor helper offsets, checks VTX strip index coverage,
and writes SMD triangles with the required winding. The QC it creates includes
the main-body IK chains required by the animation library. The supplied final
QCs additionally contain their articulated collision declarations.

The private workspace pipeline used to produce this import is retained under
`player_character_work/tools/` in the enclosing HL2BetaMegaCuck workspace:
`stage_characters.py`, `retarget_model.py`, `add_character_physics.py`,
`repair_character_ik.py`, `repair_character_winding.py`,
`repair_gasmask_assets.py`, `rebuild_gasmask_assets.py`, `rebuild_character_eyes.py`,
`validate_character_assets.py`, and
`stage_player_overlay.py`. These bulk staging tools use local owned-game paths;
the portable converter and final SMD/QC sources do not require those paths.

Original asset paths and mapping decisions are recorded in each
`retarget_report.json`. Facial flexes are not reconstructed. The rest of the
roster retains its imported compiled body geometry and uses the shared HL2DM
animation libraries.
