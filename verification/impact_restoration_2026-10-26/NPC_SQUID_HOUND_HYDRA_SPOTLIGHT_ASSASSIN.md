# Restored beta NPC runtime blockers — current batch

These changes are source repairs awaiting the coordinating agent's native build,
spawn, combat, save/load, and asset checks. Registration alone is not a pass.

- Bullsquid: defaults 120 health, 15 bite, 25 whip, and spit 5 damage/50 radius
  come from the supplied beta skill.cfg annotations. The old entries were
  commented out while code defaults were zero. Actual tailwhip event 6 now uses
  the existing whip damage variable. Headcrab prey checks use its classification
  for current and historical classnames. The previously disabled hurt/chase
  detour uses the current pathfinder. Original `hc_spot`/smell activity tags map
  to the corresponding AI actions. Its 2003 additive half-gravity blast spit is
  isolated as `grenade_bullsquid_spit` in the same compiled source file; the
  modern antlion worker's shared poison/particle projectile remains separate.
- Houndeye: original annotated defaults 80 health/25 blast; correct `energy_wave`
  precache and owner; failed effects cannot dereference null, and the wave is a
  saved entity handle. Distance attenuation cannot turn negative behind a large
  target. Existing squad/sonic/jump behavior is retained.
- Combine Assassin: the supplied fassassin model's numeric firing and kick events
  1/2/3 are accepted alongside modern named events. The original Eye and muzzle
  attachments are checked, both eye effects are precached, invalid flip sequences
  are rejected, and losing a vantage-point enemy fails the task. Footsteps follow
  half-cycles of the authored run instead of one shared static think counter.
  The provided scripts contain `FootstepLeft/Right`, not `NPC_Assassin.Footstep`.
  Original source references `player/pl_shoot1/2.wav`, but neither file exists in
  the supplied game content; firing uses its mounted beta `Weapon_Pistol.Single`
  event. This is an explicit compatible sound mapping, not recovered missing audio.
- Hydra: original networked tentacle and impale behavior retained; restored sound
  patches are reused/cleaned up, initial bone state is finite, zero-length force
  calculations are guarded, failed ragdoll/constraint allocations do not crash or
  leak helper entities. A saved target handle removes an impale helper after its
  target or Hydra disappears without touching an already-invalid constraint.
- Spotlight: original invisible point controller remains distinct from a visible
  Combot. Its dummy model is precached; beam/end allocations and removal are
  handled; byte alpha uses 255 rather than 1; static lights honor start-off and
  LightOn/LightOff even though their original no-movement mode does not think.

No VPC files or weapon values were edited in this batch. Backups and hashes are
under `C:/Users/doge6/Documents/GammaSourceTest/npc_restoration_2026-10-26/`.
The first native compile caught two missing Bullsquid includes; `ai_waypoint.h`
and `hl2_shareddefs.h` were added. Final compile/runtime status comes from the
coordinating build log and test matrix.

Two authenticity limitations remain explicit:

1. The separate original `models/Assassin.mdl` has knife/tripwire/pest/smoke clips,
   but no Alien Assassin AI implementation was found in the supplied original
   HL2/HL1 C++ trees. `npc_assassin` source is the Combine fassassin. Giving that
   same AI another model would not restore the Alien Assassin.
2. The Bullsquid model has no exact `ACT_SQUID_INSPECT_FLOOR` clip. Existing food
   inspection/wallow behavior still needs an appropriate authored animation or
   a clearly documented behavioral adaptation; no unrelated pose is substituted.

Exact recovered relationship rows for global integration are in
`modernization_v18/game_shared/hl2/hl2_gamerules.cpp`: Bullsquid 327–355, Houndeye
625–653, with reciprocal incoming relationships elsewhere in that same table.
The coordinating agent owns that matrix separately.
