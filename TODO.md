# Project TODO

This file mirrors the shared [README TODO list](README.md#todo). All unfinished work is **on hold**. Do not start or resume any items until the project owner explicitly says to **continue on the TODO list**.

- [ ] **Finish current NPC, model, animation and scene restoration.** Continue the unfinished fixes and verification in [v28 known bugs](docs/V28_KNOWN_BUGS.md), including normal choreographed body sequences on `e3_lab` and `e3_hydra`.
- [ ] **Fix barnacle pickup of newly added NPCs.** New NPCs currently cannot be picked up by barnacles.
- [ ] **Fix intermittent hard-stop crashes when killing NPCs.** NPC deaths sometimes cause the entire game to crash.
- [ ] **Restore the Houndeye's missing sonic shockwave.** Consider adapting the Half-Life 1 effect and polishing it to fit Half-Life 2's visual style.
- [ ] **Complete full ports of the unfinished cut Combine enemies.** Most remain substantially unfinished and need complete behavior and presentation.
- [ ] **Complete `zoo_item` visual verification.** Inspect the sky, original HEV pickup and physgun beam in an isolated test profile.
- [ ] **Runtime-test friendly fire.** Verify `gamma_friendlyfire` across the intended allied-damage paths.
- [ ] **Measure stability and performance.** Record frame times, memory use, map-loading times and long-session crashes.
- [ ] **Remove every Steam dependency.** Cover the game, engine, build process, tools, launchers, content lookup and runtime services; verify independent builds and runtime operation.
- [ ] **Replace normal Hammer with a WorldCraft-styled editor combining both feature sets.** Reverse-engineer the supplied `hammerplusplus_2013sp_build8872.zip` and combine its features with the existing editor. Match the original 2003 WorldCraft appearance and branding exactly. Start this after Steam dependency removal.
- [ ] **Repair .357/crossbow presentation and restore ammo for every added weapon.** Fix stretching of the mandatory 2003 hands, use the Half-Life 2 crossbow logo/icon, restore ammo availability and pickups for all added weapons, and use .357 ammo models for `weapon_proto1` ammo pickups. Preserve the already repaired weapon values.

Preserve original beta HUD, weapons, hands, animations, NPCs, interface, ground deformation, maps and effects throughout this work.
