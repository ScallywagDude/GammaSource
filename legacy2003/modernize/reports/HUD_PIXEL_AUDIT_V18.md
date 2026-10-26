# v18 HUD pixel audit

Actual local VTF pixel data was decoded without changing source assets. All stored frames and mip levels of supported non-cubemap VTF 7.0/7.1 textures were decoded; unsupported formats remain explicit. Rectangles were checked against decoded dimensions and tested for effective nonblank pixels using the material alpha/additive rule.

Contact sheets preserve icon aspect ratio and use nearest-neighbor pixels over a checkerboard. They are diagnostic derivatives outside the source archive. No placeholder icons or replacement artwork were generated.

The sprite parser mirrors the final restored source behavior: a role with a `font` key is unsupported; a nonempty direct `file` wins unchanged; otherwise a nonempty legacy child `640` file is preferred over `320`. A selected child with a `font` key is also unsupported. Only selected legacy leaf-only file names are prefixed with `sprites/`; slash/backslash/colon-qualified and all direct paths remain unchanged. Nonempty invalid chosen metadata is reported without silently selecting a fallback. Chosen legacy blocks are recorded and remain runtime-unverified. Font entries are explicitly unverified, not certified as decoded icons.

**Limits:** nonblank pixels do not prove the artwork depicts the correct weapon. Image-to-model matching, semantic icon identity, VGUI runtime scale, blending/gamma on the GPU, animation appearance and actual game rendering remain unverified. Content touching a crop boundary is recorded as a review hint, not proof of cropping. Uniform colors are recorded without declaring them an artwork defect.

## hl2

Scripts inspected: 42; source-registered scripts among these: 32; source-registered classes in inventory: 32; referenced textures: 11; whole textures decoded: 11.

Scripts are asset evidence and do not establish a supported source weapon. Script-only entries: `weapon_357`, `weapon_alyxgun`, `weapon_annabelle`, `weapon_beerbottle`, `weapon_beerbottle2`, `weapon_brickbat`, `weapon_citizenpackage`, `weapon_citizensuitcase`, `weapon_crossbow`, `weapon_ml`.

| Pixel/rectangle state | Icons |
|---|---:|
| font_glyph_unverified_source_unsupported | 21 |
| pixels_verified_nonblank | 212 |

| Weapon | Source registration | Role | Configuration | Rectangle | Result |
|---|---|---|---|---|---|
| weapon_357 | script only | ammo | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_357 | script only | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_357 | script only | crosshair | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_357 | script only | weapon | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_357 | script only | weapon_s | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_357 | script only | weapon_small | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_alyxgun | script only | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_alyxgun | script only | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_alyxgun | script only | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_alyxgun | script only | weapon | direct | [128, 192, 128, 64] | pixels_verified_nonblank |
| weapon_alyxgun | script only | weapon_s | direct | [128, 192, 128, 64] | pixels_verified_nonblank |
| weapon_annabelle | script only | ammo | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_annabelle | script only | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_annabelle | script only | crosshair | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_annabelle | script only | weapon | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_annabelle | script only | weapon_s | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_annabelle | script only | weapon_small | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_ar1 | registered | ammo | direct | [55, 45, 73, 15] | pixels_verified_nonblank |
| weapon_ar1 | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_ar1 | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_ar1 | registered | weapon | direct | [0, 0, 128, 64] | pixels_verified_nonblank |
| weapon_ar1 | registered | weapon_s | direct | [0, 0, 128, 64] | pixels_verified_nonblank |
| weapon_ar2 | registered | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_ar2 | registered | ammo2 | direct | [55, 110, 73, 20] | pixels_verified_nonblank |
| weapon_ar2 | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_ar2 | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_ar2 | registered | weapon | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_ar2 | registered | weapon_s | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_beerbottle | script only | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_beerbottle | script only | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_beerbottle | script only | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_beerbottle | script only | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_beerbottle | script only | weapon | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_beerbottle | script only | weapon_s | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_beerbottle2 | script only | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_beerbottle2 | script only | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_beerbottle2 | script only | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_beerbottle2 | script only | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_beerbottle2 | script only | weapon | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_beerbottle2 | script only | weapon_s | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_binoculars | registered | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_binoculars | registered | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_binoculars | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_binoculars | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_binoculars | registered | weapon | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_binoculars | registered | weapon_s | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_binoculars | registered | zoom | direct | [0, 0, 128, 128] | pixels_verified_nonblank |
| weapon_binoculars | registered | zoom_autoaim | direct | [0, 0, 128, 128] | pixels_verified_nonblank |
| weapon_brickbat | script only | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_brickbat | script only | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_brickbat | script only | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_brickbat | script only | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_brickbat | script only | weapon | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_brickbat | script only | weapon_s | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_bugbait | registered | ammo | direct | [55, 110, 73, 20] | pixels_verified_nonblank |
| weapon_bugbait | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_bugbait | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_bugbait | registered | weapon | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_bugbait | registered | weapon_s | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_cguard | registered | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_cguard | registered | ammo2 | direct | [55, 110, 73, 20] | pixels_verified_nonblank |
| weapon_cguard | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_cguard | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_cguard | registered | weapon | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_cguard | registered | weapon_s | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_citizenpackage | script only | ammo | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_citizenpackage | script only | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_citizenpackage | script only | crosshair | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_citizenpackage | script only | weapon | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_citizenpackage | script only | weapon_s | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_citizenpackage | script only | weapon_small | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_citizensuitcase | script only | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_citizensuitcase | script only | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_citizensuitcase | script only | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_citizensuitcase | script only | weapon | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_citizensuitcase | script only | weapon_s | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_crossbow | script only | ammo | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_crossbow | script only | autoaim | direct | [0, 96, 24, 24] | pixels_verified_nonblank |
| weapon_crossbow | script only | crosshair | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_crossbow | script only | weapon | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_crossbow | script only | weapon_s | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_crossbow | script only | weapon_small | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_crossbow | script only | zoom | font_role_source_unsupported |  | font_glyph_unverified_source_unsupported |
| weapon_crossbow | script only | zoom_autoaim | direct | [24, 112, 104, 16] | pixels_verified_nonblank |
| weapon_crowbar | registered | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_crowbar | registered | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_crowbar | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_crowbar | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_crowbar | registered | weapon | direct | [0, 0, 128, 64] | pixels_verified_nonblank |
| weapon_crowbar | registered | weapon_s | direct | [0, 0, 128, 64] | pixels_verified_nonblank |
| weapon_cubemap | registered | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_cubemap | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_cubemap | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_cubemap | registered | weapon | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_cubemap | registered | weapon_s | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_extinguisher | registered | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_extinguisher | registered | ammo2 | direct | [55, 110, 73, 20] | pixels_verified_nonblank |
| weapon_extinguisher | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_extinguisher | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_extinguisher | registered | weapon | direct | [0, 0, 128, 64] | pixels_verified_nonblank |
| weapon_extinguisher | registered | weapon_s | direct | [0, 0, 128, 64] | pixels_verified_nonblank |
| weapon_flaregun | registered | ammo | direct | [55, 130, 73, 20] | pixels_verified_nonblank |
| weapon_flaregun | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_flaregun | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_flaregun | registered | weapon | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_flaregun | registered | weapon_s | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_frag | registered | ammo | direct | [55, 110, 73, 20] | pixels_verified_nonblank |
| weapon_frag | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_frag | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_frag | registered | weapon | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_frag | registered | weapon_s | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_gauss | registered | ammo | direct | [55, 130, 73, 20] | pixels_verified_nonblank |
| weapon_gauss | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_gauss | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_gauss | registered | weapon | direct | [0, 0, 128, 64] | pixels_verified_nonblank |
| weapon_gauss | registered | weapon_s | direct | [0, 0, 128, 64] | pixels_verified_nonblank |
| weapon_hmg1 | registered | ammo | direct | [55, 45, 73, 15] | pixels_verified_nonblank |
| weapon_hmg1 | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_hmg1 | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_hmg1 | registered | weapon | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_hmg1 | registered | weapon_s | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_hopwire | registered | ammo | legacy_resolution_640 | [55, 110, 73, 20] | pixels_verified_nonblank |
| weapon_hopwire | registered | autoaim | legacy_resolution_640 | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_hopwire | registered | crosshair | legacy_resolution_640 | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_hopwire | registered | weapon | direct | [0, 192, 128, 64] | pixels_verified_nonblank |
| weapon_hopwire | registered | weapon_s | direct | [0, 192, 128, 64] | pixels_verified_nonblank |
| weapon_iceaxe | registered | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_iceaxe | registered | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_iceaxe | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_iceaxe | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_iceaxe | registered | weapon | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_iceaxe | registered | weapon_s | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_immolator | registered | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_immolator | registered | ammo2 | direct | [55, 110, 73, 20] | pixels_verified_nonblank |
| weapon_immolator | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_immolator | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_immolator | registered | weapon | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_immolator | registered | weapon_s | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_irifle | registered | ammo | direct | [0, 0, 128, 30] | pixels_verified_nonblank |
| weapon_irifle | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_irifle | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_irifle | registered | weapon | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_irifle | registered | weapon_s | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_manhack | registered | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_manhack | registered | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_manhack | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_manhack | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_manhack | registered | weapon | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_manhack | registered | weapon_s | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_ml | script only | ammo | direct | [0, 0, 128, 30] | pixels_verified_nonblank |
| weapon_ml | script only | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_ml | script only | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_ml | script only | weapon | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_ml | script only | weapon_s | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_molotov | registered | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_molotov | registered | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_molotov | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_molotov | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_molotov | registered | weapon | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_molotov | registered | weapon_s | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_physcannon | registered | ammo | direct | [0, 0, 32, 32] | pixels_verified_nonblank |
| weapon_physcannon | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_physcannon | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_physcannon | registered | weapon | direct | [128, 192, 128, 64] | pixels_verified_nonblank |
| weapon_physcannon | registered | weapon_s | direct | [128, 192, 128, 64] | pixels_verified_nonblank |
| weapon_physgun | registered | ammo | direct | [0, 0, 32, 32] | pixels_verified_nonblank |
| weapon_physgun | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_physgun | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_physgun | registered | weapon | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_physgun | registered | weapon_s | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_pistol | registered | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_pistol | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_pistol | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_pistol | registered | weapon | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_pistol | registered | weapon_s | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_rollerwand | registered | ammo | direct | [0, 0, 32, 32] | pixels_verified_nonblank |
| weapon_rollerwand | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_rollerwand | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_rollerwand | registered | weapon | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_rollerwand | registered | weapon_s | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_rpg | registered | ammo | direct | [0, 0, 128, 30] | pixels_verified_nonblank |
| weapon_rpg | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_rpg | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_rpg | registered | weapon | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_rpg | registered | weapon_s | direct | [0, 64, 128, 64] | pixels_verified_nonblank |
| weapon_shotgun | registered | ammo | direct | [55, 90, 73, 20] | pixels_verified_nonblank |
| weapon_shotgun | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_shotgun | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_shotgun | registered | weapon | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_shotgun | registered | weapon_s | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_slam | registered | ammo | direct | [0, 0, 32, 32] | pixels_verified_nonblank |
| weapon_slam | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_slam | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_slam | registered | weapon | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_slam | registered | weapon_s | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_smg1 | registered | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_smg1 | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_smg1 | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_smg1 | registered | weapon | direct | [128, 192, 128, 64] | pixels_verified_nonblank |
| weapon_smg1 | registered | weapon_s | direct | [0, 128, 128, 64] | pixels_verified_nonblank |
| weapon_smg2 | registered | ammo | direct | [55, 75, 73, 15] | pixels_verified_nonblank |
| weapon_smg2 | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_smg2 | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_smg2 | registered | weapon | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_smg2 | registered | weapon_s | direct | [128, 128, 128, 64] | pixels_verified_nonblank |
| weapon_sniperrifle | registered | ammo | direct | [55, 30, 73, 15] | pixels_verified_nonblank |
| weapon_sniperrifle | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_sniperrifle | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_sniperrifle | registered | weapon | direct | [0, 192, 128, 64] | pixels_verified_nonblank |
| weapon_sniperrifle | registered | weapon_s | direct | [0, 192, 128, 64] | pixels_verified_nonblank |
| weapon_sniperrifle | registered | zoom | direct | [0, 0, 128, 128] | pixels_verified_nonblank |
| weapon_sniperrifle | registered | zoom_autoaim | direct | [0, 0, 128, 128] | pixels_verified_nonblank |
| weapon_stickylauncher | registered | ammo | direct | [55, 60, 73, 15] | pixels_verified_nonblank |
| weapon_stickylauncher | registered | autoaim | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_stickylauncher | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_stickylauncher | registered | weapon | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_stickylauncher | registered | weapon_s | direct | [128, 0, 128, 64] | pixels_verified_nonblank |
| weapon_stunstick | registered | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_stunstick | registered | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_stunstick | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_stunstick | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_stunstick | registered | weapon | direct | [0, 192, 128, 64] | pixels_verified_nonblank |
| weapon_stunstick | registered | weapon_s | direct | [0, 192, 128, 64] | pixels_verified_nonblank |
| weapon_thumper | registered | ammo | direct | [0, 0, 32, 32] | pixels_verified_nonblank |
| weapon_thumper | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_thumper | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_thumper | registered | weapon | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_thumper | registered | weapon_s | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_tripwire | registered | ammo | direct | [0, 0, 32, 32] | pixels_verified_nonblank |
| weapon_tripwire | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_tripwire | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_tripwire | registered | weapon | direct | [128, 64, 128, 64] | pixels_verified_nonblank |
| weapon_tripwire | registered | weapon_s | direct | [128, 64, 128, 64] | pixels_verified_nonblank |

Local diagnostic sheets (excluded from the source ZIP):

- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_weapon_icons_01.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_weapon_icons_02.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_weapon_icons_03.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_weapon_icons_04.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_weapon_icons_05.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_weapon_icons_06.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_weapon_icons_07.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_weapon_icons_08.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_atlases_01.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl2_atlases_02.png`

## hl1

Scripts inspected: 15; source-registered scripts among these: 15; source-registered classes in inventory: 15; referenced textures: 11; whole textures decoded: 11.

Scripts are asset evidence and do not establish a supported source weapon. Script-only entries: none.

| Pixel/rectangle state | Icons |
|---|---:|
| blank_icon_pixels | 3 |
| pixels_verified_nonblank | 61 |

| Weapon | Source registration | Role | Configuration | Rectangle | Result |
|---|---|---|---|---|---|
| weapon_357 | registered | ammo | direct | [24, 72, 24, 24] | pixels_verified_nonblank |
| weapon_357 | registered | autoaim | direct | [24, 72, 24, 24] | blank_icon_pixels |
| weapon_357 | registered | crosshair | direct | [48, 0, 24, 24] | pixels_verified_nonblank |
| weapon_357 | registered | weapon | direct | [0, 90, 170, 45] | pixels_verified_nonblank |
| weapon_357 | registered | weapon_s | direct | [0, 90, 170, 45] | pixels_verified_nonblank |
| weapon_357 | registered | zoom | direct | [96, 0, 24, 24] | pixels_verified_nonblank |
| weapon_crossbow | registered | ammo | direct | [96, 72, 24, 24] | pixels_verified_nonblank |
| weapon_crossbow | registered | autoaim | direct | [0, 96, 24, 24] | pixels_verified_nonblank |
| weapon_crossbow | registered | crosshair | direct | [72, 0, 24, 24] | pixels_verified_nonblank |
| weapon_crossbow | registered | weapon | direct | [0, 0, 170, 45] | pixels_verified_nonblank |
| weapon_crossbow | registered | weapon_s | direct | [0, 0, 170, 45] | pixels_verified_nonblank |
| weapon_crossbow | registered | zoom | direct | [24, 96, 104, 16] | pixels_verified_nonblank |
| weapon_crossbow | registered | zoom_autoaim | direct | [24, 112, 104, 16] | pixels_verified_nonblank |
| weapon_crowbar | registered | weapon | direct | [0, 0, 170, 45] | pixels_verified_nonblank |
| weapon_crowbar | registered | weapon_s | direct | [0, 0, 170, 45] | pixels_verified_nonblank |
| weapon_egon | registered | ammo | direct | [0, 96, 24, 24] | pixels_verified_nonblank |
| weapon_egon | registered | crosshair | direct | [72, 48, 24, 24] | pixels_verified_nonblank |
| weapon_egon | registered | weapon | direct | [0, 135, 170, 45] | pixels_verified_nonblank |
| weapon_egon | registered | weapon_s | direct | [0, 135, 170, 45] | pixels_verified_nonblank |
| weapon_gauss | registered | ammo | direct | [0, 96, 24, 24] | pixels_verified_nonblank |
| weapon_gauss | registered | crosshair | direct | [48, 48, 24, 24] | pixels_verified_nonblank |
| weapon_gauss | registered | weapon | direct | [0, 90, 170, 45] | pixels_verified_nonblank |
| weapon_gauss | registered | weapon_s | direct | [0, 90, 170, 45] | pixels_verified_nonblank |
| weapon_glock | registered | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_glock | registered | autoaim | direct | [0, 72, 24, 24] | blank_icon_pixels |
| weapon_glock | registered | crosshair | direct | [24, 0, 24, 24] | pixels_verified_nonblank |
| weapon_glock | registered | weapon | direct | [0, 45, 170, 45] | pixels_verified_nonblank |
| weapon_glock | registered | weapon_s | direct | [0, 45, 170, 45] | pixels_verified_nonblank |
| weapon_handgrenade | registered | ammo | direct | [48, 96, 24, 24] | pixels_verified_nonblank |
| weapon_handgrenade | registered | weapon | direct | [0, 0, 170, 45] | pixels_verified_nonblank |
| weapon_handgrenade | registered | weapon_s | direct | [0, 0, 170, 45] | pixels_verified_nonblank |
| weapon_hornetgun | registered | ammo | direct | [24, 96, 24, 24] | pixels_verified_nonblank |
| weapon_hornetgun | registered | crosshair | direct | [72, 24, 24, 24] | pixels_verified_nonblank |
| weapon_hornetgun | registered | weapon | direct | [0, 180, 170, 45] | pixels_verified_nonblank |
| weapon_hornetgun | registered | weapon_s | direct | [0, 180, 170, 45] | pixels_verified_nonblank |
| weapon_mp5 | registered | ammo | direct | [0, 72, 24, 24] | pixels_verified_nonblank |
| weapon_mp5 | registered | ammo2 | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_mp5 | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_mp5 | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_mp5 | registered | weapon | direct | [0, 135, 170, 45] | pixels_verified_nonblank |
| weapon_mp5 | registered | weapon_s | direct | [0, 135, 170, 45] | pixels_verified_nonblank |
| weapon_physgun | registered | ammo | direct | [0, 0, 32, 32] | pixels_verified_nonblank |
| weapon_physgun | registered | autoaim | direct | [48, 72, 24, 24] | pixels_verified_nonblank |
| weapon_physgun | registered | crosshair | direct | [0, 48, 24, 24] | pixels_verified_nonblank |
| weapon_physgun | registered | weapon | direct | [128, 192, 128, 64] | pixels_verified_nonblank |
| weapon_physgun | registered | weapon_s | direct | [128, 192, 128, 64] | pixels_verified_nonblank |
| weapon_rpg | registered | ammo | direct | [120, 72, 24, 24] | pixels_verified_nonblank |
| weapon_rpg | registered | crosshair | direct | [24, 48, 24, 24] | pixels_verified_nonblank |
| weapon_rpg | registered | weapon | direct | [0, 45, 170, 45] | pixels_verified_nonblank |
| weapon_rpg | registered | weapon_s | direct | [0, 45, 170, 45] | pixels_verified_nonblank |
| weapon_satchel | registered | ammo | direct | [72, 96, 24, 24] | pixels_verified_nonblank |
| weapon_satchel | registered | weapon | direct | [0, 45, 170, 45] | pixels_verified_nonblank |
| weapon_satchel | registered | weapon_s | direct | [0, 45, 170, 45] | pixels_verified_nonblank |
| weapon_shotgun | registered | ammo | direct | [72, 72, 24, 24] | pixels_verified_nonblank |
| weapon_shotgun | registered | autoaim | direct | [72, 72, 24, 24] | blank_icon_pixels |
| weapon_shotgun | registered | crosshair | direct | [48, 24, 24, 24] | pixels_verified_nonblank |
| weapon_shotgun | registered | weapon | direct | [0, 180, 170, 45] | pixels_verified_nonblank |
| weapon_shotgun | registered | weapon_s | direct | [0, 180, 170, 45] | pixels_verified_nonblank |
| weapon_snark | registered | ammo | direct | [96, 96, 24, 24] | pixels_verified_nonblank |
| weapon_snark | registered | weapon | direct | [0, 135, 170, 45] | pixels_verified_nonblank |
| weapon_snark | registered | weapon_s | direct | [0, 135, 170, 45] | pixels_verified_nonblank |
| weapon_tripmine | registered | ammo | direct | [120, 96, 24, 24] | pixels_verified_nonblank |
| weapon_tripmine | registered | weapon | direct | [0, 90, 170, 45] | pixels_verified_nonblank |
| weapon_tripmine | registered | weapon_s | direct | [0, 90, 170, 45] | pixels_verified_nonblank |

Local diagnostic sheets (excluded from the source ZIP):

- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl1_weapon_icons_01.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl1_weapon_icons_02.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl1_weapon_icons_03.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl1_atlases_01.png`
- `C:\Users\doge6\Documents\HL2BetaMegaCuck\asset_visual_audit_v18\hl1_atlases_02.png`

