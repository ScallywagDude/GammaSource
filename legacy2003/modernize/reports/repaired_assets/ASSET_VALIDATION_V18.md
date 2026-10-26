# v18 content validation

This report verifies source structure, exact path casing, parsed headers, and dependency references. It does not verify compilation, gameplay, UV alignment, animation quality, or visual artwork identity.

The source archive contains no game MDL, VVD, VTX, PHY, QC, SMD, VMT, VTF, weapon scripts, or sounds. External local asset roots are read only and are not included in either v18 archive.

## hl2

Search roots: C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_restoration_v18\hl2\root0, C:\Users\doge6\Documents\HL2BetaMegaCuck\hl2_2003, C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_restoration_v18\hl2\root1, C:\Users\doge6\Documents\HL2BetaMegaCuck\hl2

Weapons: 32; models inspected/referenced: 132; materials: 8363; textures: 8353.

| Classname | Script | Viewmodel | World model | HUD |
|---|---|---|---|---|
| weapon_ar1 | scripts/weapon_ar1.txt [verified_file] | models/Weapons/v_ar1.mdl [verified_file] | models/Weapons/w_ar1.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_ar2 | scripts/weapon_ar2.txt [verified_file] | models/Weapons/v_ar2.mdl [verified_file] | models/Weapons/w_AR2.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_binoculars | scripts/weapon_binoculars.txt [verified_file] | models/Weapons/v_Binoculars.mdl [verified_file] | models/Weapons/w_binoculars.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle; zoom: verified_rectangle; zoom_autoaim: verified_rectangle |
| weapon_bugbait | scripts/weapon_bugbait.txt [verified_file] | models/Weapons/V_bugbait.mdl [verified_file] | models/spore.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_cguard | scripts/weapon_cguard.txt [verified_file] | models/Weapons/v_Guardgun.mdl [verified_file] | models/Weapons/w_guardgun.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_crowbar | scripts/weapon_crowbar.txt [verified_file] | models/Weapons/v_crowbar.mdl [verified_file] | models/Weapons/w_crowbar.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_cubemap | scripts/weapon_cubemap.txt [verified_file] | models/ShaderTest/envballs.mdl [verified_file] | models/ShaderTest/envballs.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_extinguisher | scripts/weapon_extinguisher.txt [verified_file] | models/Weapons/v_fire_extinguisher.mdl [verified_file] | models/Weapons/W_Fire_Extinguisher.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_flaregun | scripts/weapon_flaregun.txt [verified_file] | models/Weapons/v_flaregun.mdl [verified_file] | models/Weapons/w_flaregun.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_frag | scripts/weapon_frag.txt [verified_file] | models/Weapons/v_Grenade.mdl [verified_file] | models/Weapons/w_grenade.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_gauss | scripts/weapon_gauss.txt [verified_file] | models/Weapons/v_Gauss.mdl [verified_file] | models/Weapons/w_gauss.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_hmg1 | scripts/weapon_hmg1.txt [verified_file] | models/Weapons/v_HMG.mdl [verified_file] | models/Weapons/w_hmg1.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_hopwire | scripts/weapon_hopwire.txt [verified_file] | models/Weapons/V_hopwire.mdl [verified_file] | models/Weapons/w_hopwire.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_iceaxe | scripts/weapon_iceaxe.txt [verified_file] | models/Weapons/v_iceaxe.mdl [verified_file] | models/Weapons/w_iceaxe.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_immolator | scripts/weapon_immolator.txt [verified_file] | models/Weapons/v_Cremator.mdl [verified_file] | models/Weapons/w_immolator.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_irifle | scripts/weapon_irifle.txt [verified_file] | models/Weapons/v_IRifle.mdl [verified_file] | models/Weapons/w_IRifle.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_manhack | scripts/weapon_manhack.txt [verified_file] | models/Weapons/v_Manhack.mdl [verified_file] | models/Weapons/v_Manhack.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_molotov | scripts/weapon_molotov.txt [verified_file] | models/Weapons/V_molotov.mdl [verified_file] | models/Weapons/w_molotov.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_physcannon | scripts/weapon_physcannon.txt [verified_file] | models/Weapons/v_physcannon.mdl [verified_file] | models/Weapons/W_physics_gun.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_physgun | scripts/weapon_physgun.txt [verified_file] | models/Weapons/v_Physics.mdl [verified_file] | models/Weapons/w_physics.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_pistol | scripts/weapon_pistol.txt [verified_file] | models/Weapons/v_Pistol.mdl [verified_file] | models/Weapons/W_pistol.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_rollerwand | scripts/weapon_rollerwand.txt [verified_file] | models/Weapons/v_slam.mdl [verified_file] | models/Weapons/w_slam.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_rpg | scripts/weapon_rpg.txt [verified_file] | models/Weapons/v_RPG.mdl [verified_file] | models/Weapons/w_rocket_launcher.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_shotgun | scripts/weapon_shotgun.txt [verified_file] | models/Weapons/v_shotgun.mdl [verified_file] | models/Weapons/w_shotgun.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_slam | scripts/weapon_slam.txt [verified_file] | models/Weapons/v_slam.mdl [verified_file] | models/Weapons/w_slam.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_smg1 | scripts/weapon_smg1.txt [verified_file] | models/Weapons/v_smg1.mdl [verified_file] | models/Weapons/w_smg1.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_smg2 | scripts/weapon_smg2.txt [verified_file] | models/Weapons/v_smg2.mdl [verified_file] | models/Weapons/w_smg2.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_sniperrifle | scripts/weapon_sniperrifle.txt [verified_file] | models/Weapons/v_sniper.mdl [verified_file] | models/Weapons/w_sniper.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle; zoom: verified_rectangle; zoom_autoaim: verified_rectangle |
| weapon_stickylauncher | scripts/weapon_stickylauncher.txt [verified_file] | models/Weapons/v_Pistol.mdl [verified_file] | models/Weapons/W_pistol.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_stunstick | scripts/weapon_stunstick.txt [verified_file] | models/Weapons/V_Stunbaton.mdl [verified_file] | models/Weapons/w_stunbaton.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_thumper | scripts/weapon_thumper.txt [verified_file] | models/Weapons/v_slam.mdl [verified_file] | models/Fire_equipment/W_FireHydrant.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_tripwire | scripts/weapon_tripwire.txt [verified_file] | models/Weapons/v_slam.mdl [verified_file] | models/Weapons/w_slam.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |

Material assignments, dimensions, sequence names, companions, sounds, source build inclusion, and detailed dependency results are recorded in ASSET_VALIDATION_V18.json.

## hl1

Search roots: C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_restoration_v18\hl1\root0, C:\Users\doge6\Documents\HL2BetaMegaCuck\hl1

Weapons: 15; models inspected/referenced: 33; materials: 7053; textures: 6995.

| Classname | Script | Viewmodel | World model | HUD |
|---|---|---|---|---|
| weapon_357 | scripts/weapon_357.txt [verified_file] | models/v_357.mdl [verified_file] | models/w_357.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle; zoom: verified_rectangle |
| weapon_crossbow | scripts/weapon_crossbow.txt [verified_file] | models/v_crossbow.mdl [verified_file] | models/w_crossbow.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle; zoom: verified_rectangle; zoom_autoaim: verified_rectangle |
| weapon_crowbar | scripts/weapon_crowbar.txt [verified_file] | models/v_crowbar.mdl [verified_file] | models/w_crowbar.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle |
| weapon_egon | scripts/weapon_egon.txt [verified_file] | models/v_egon.mdl [verified_file] | models/w_egon.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle |
| weapon_gauss | scripts/weapon_gauss.txt [verified_file] | models/v_gauss.mdl [verified_file] | models/w_gauss.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle |
| weapon_glock | scripts/weapon_glock.txt [verified_file] | models/v_9mmhandgun.mdl [verified_file] | models/w_9mmhandgun.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_handgrenade | scripts/weapon_handgrenade.txt [verified_file] | models/v_grenade.mdl [verified_file] | models/w_grenade.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle |
| weapon_hornetgun | scripts/weapon_hornetgun.txt [verified_file] | models/v_HGun.mdl [verified_file] | models/w_hgun.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle |
| weapon_mp5 | scripts/weapon_mp5.txt [verified_file] | models/v_9mmAR.mdl [verified_file] | models/w_9mmAR.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; ammo2: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_physgun | scripts/weapon_physgun.txt [verified_file] | models/v_egon.mdl [verified_file] | models/w_egon.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_rpg | scripts/weapon_rpg.txt [verified_file] | models/v_rpg.mdl [verified_file] | models/w_rpg.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle |
| weapon_satchel | scripts/weapon_satchel.txt [verified_file] | models/v_satchel.mdl [verified_file] | models/w_satchel.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle |
| weapon_shotgun | scripts/weapon_shotgun.txt [verified_file] | models/v_shotgun.mdl [verified_file] | models/w_shotgun.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle; crosshair: verified_rectangle; autoaim: verified_rectangle |
| weapon_snark | scripts/weapon_snark.txt [verified_file] | models/v_squeak.mdl [verified_file] | models/w_squeak.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle |
| weapon_tripmine | scripts/weapon_tripmine.txt [verified_file] | models/v_tripmine.mdl [verified_file] | models/w_tripmine.mdl [verified_file] | weapon: verified_rectangle; weapon_s: verified_rectangle; ammo: verified_rectangle |

Material assignments, dimensions, sequence names, companions, sounds, source build inclusion, and detailed dependency results are recorded in ASSET_VALIDATION_V18.json.

## cstrike

Search roots: none (unverified)

Weapons: 28; models inspected/referenced: 1; materials: 1; textures: 0.

| Classname | Script | Viewmodel | World model | HUD |
|---|---|---|---|---|
| weapon_ak47 | scripts/weapon_ak47.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_aug | scripts/weapon_aug.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_awp | scripts/weapon_awp.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_basecsgrenade | scripts/weapon_basecsgrenade.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_c4 | scripts/weapon_c4.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_cs_base | scripts/weapon_cs_base.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_csbase_gun | scripts/weapon_csbase_gun.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_deagle | scripts/weapon_deagle.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_famas | scripts/weapon_famas.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_fiveseven | scripts/weapon_fiveseven.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_flashbang | scripts/weapon_flashbang.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_g3sg1 | scripts/weapon_g3sg1.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_galil | scripts/weapon_galil.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_glock | scripts/weapon_glock.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_hegrenade | scripts/weapon_hegrenade.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_knife | scripts/weapon_knife.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_m249 | scripts/weapon_m249.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_m3 | scripts/weapon_m3.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_m4a1 | scripts/weapon_m4a1.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_mac10 | scripts/weapon_mac10.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_mp5navy | scripts/weapon_mp5navy.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_p228 | scripts/weapon_p228.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_p90 | scripts/weapon_p90.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_scout | scripts/weapon_scout.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_sg550 | scripts/weapon_sg550.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_sg552 | scripts/weapon_sg552.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_ump45 | scripts/weapon_ump45.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_usp | scripts/weapon_usp.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |

Material assignments, dimensions, sequence names, companions, sounds, source build inclusion, and detailed dependency results are recorded in ASSET_VALIDATION_V18.json.

## tf2

Search roots: none (unverified)

Weapons: 38; models inspected/referenced: 4; materials: 9; textures: 0.

| Classname | Script | Viewmodel | World model | HUD |
|---|---|---|---|---|
| weapon_arcwelder | scripts/weapon_arcwelder.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_basecombatobject | scripts/weapon_basecombatobject.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_builder | scripts/weapon_builder.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_basegrenade | scripts/weapon_combat_basegrenade.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_burstrifle | scripts/weapon_combat_burstrifle.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_chargeableplasma | scripts/weapon_combat_chargeableplasma.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_grenade | scripts/weapon_combat_grenade.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_grenade_emp | scripts/weapon_combat_grenade_emp.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_laserrifle | scripts/weapon_combat_laserrifle.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_plasmagrenadelauncher | scripts/weapon_combat_plasmagrenadelauncher.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_plasmarifle | scripts/weapon_combat_plasmarifle.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_plasmarifle_alien | scripts/weapon_combat_plasmarifle_alien.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_plasmarifle_base | scripts/weapon_combat_plasmarifle_base.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_shield | scripts/weapon_combat_shield.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_shield_alien | scripts/weapon_combat_shield_alien.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_shotgun | scripts/weapon_combat_shotgun.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_combat_usedwithshieldbase | scripts/weapon_combat_usedwithshieldbase.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_drainbeam | scripts/weapon_drainbeam.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_flame_thrower | scripts/weapon_flame_thrower.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_gas_can | scripts/weapon_gas_can.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_grenade_rocket | scripts/weapon_grenade_rocket.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_harpoon | scripts/weapon_harpoon.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_infiltrator | scripts/weapon_infiltrator.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_laserdesignator | scripts/weapon_laserdesignator.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_laserrifle | scripts/weapon_laserrifle.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_limpetmine | scripts/weapon_limpetmine.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_minigun | scripts/weapon_minigun.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_mortar | scripts/weapon_mortar.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_obj_empgenerator | scripts/weapon_obj_empgenerator.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_obj_rallyflag | scripts/weapon_obj_rallyflag.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_objectselection | scripts/weapon_objectselection.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_pistols | scripts/weapon_pistols.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_plasmarifle | scripts/weapon_plasmarifle.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_repairgun | scripts/weapon_repairgun.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_rocket_launcher | scripts/weapon_rocket_launcher.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_shield | scripts/weapon_shield.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_shield_grenade | scripts/weapon_shield_grenade.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |
| weapon_twohandedcontainer | scripts/weapon_twohandedcontainer.txt [unverified_no_asset_directory] |  [unverified] |  [unverified] | unverified |

Material assignments, dimensions, sequence names, companions, sounds, source build inclusion, and detailed dependency results are recorded in ASSET_VALIDATION_V18.json.

## Outstanding content problems

1405 distinct contextual problems; see MISSING_ASSETS_V18.json for exact paths and reasons.

The missing list is a dependency closure of the audited weapons, NPCs, models and effects, not a claim that every map or dynamically constructed engine resource was analyzed.
