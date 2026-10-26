# Project TODO

These future tasks were requested by the user on 26 October 2026. They are queued after the current restoration work; Steam dependency removal precedes the Hammer++ replacement.

- [ ] **Remove every Steam dependency.** Audit and remove Steam dependencies across the game, engine, build process, tools, launchers, content lookup and runtime services. Verify the complete game and tools can build and run independently of Steam.

- [ ] **Replace normal Hammer with a WorldCraft-styled editor combining both feature sets.** Reverse-engineer the supplied Hammer++ build and integrate its features alongside the existing editor's features. Match the original 2003 WorldCraft appearance exactly, retaining its WorldCraft branding, menus, layout, icons and styling. Reference archive: `C:\Users\doge6\Downloads\hammerplusplus_2013sp_build8872.zip`. Start this after Steam dependency removal.

- [ ] **Repair .357 and crossbow presentation, and restore ammo for every added weapon.** Fix stretching of the mandatory 2003 hands on the .357 and crossbow. Replace the crossbow's Half-Life 1 logo/icon with the Half-Life 2 logo/icon. Restore ammo availability and pickups for **all added weapons**, as clarified by the user; preserve the already repaired weapon values. Use the **.357 ammo models** for `weapon_proto1` ammo pickups.
