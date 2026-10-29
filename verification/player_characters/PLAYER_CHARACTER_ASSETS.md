# Selectable player character assets

The character registry contains 16 models, installed under
`gammasource_overlay/models/gamma/player/`. The `gamma_player_model` user setting
accepts the aliases below. `gamma_characters` opens the in-game selector.

| Alias | Original asset source |
| --- | --- |
| `gordon` | Survivor-derived Gordon Freeman community player port, [Gamebanana distribution](https://gamebanana.com/mods/652235) |
| `chell` | Installed Portal: `models/player/chell.mdl` |
| `chell_2010` | User's July 2010 Portal 2 build: `portal2/models/player/chell/player.mdl` |
| `mel` | User's July 2009 Portal 2 build: `portal2/models/player.mdl`, blonde skin family 1 |
| `gasmask_citizen` | User's converted legacy `models/player/citizen17.mdl` and original Citizen_17 textures |
| `hl2dm_rebel`, `hl2dm_female`, `hl2dm_combine` | Installed Half-Life 2 Deathmatch models and male/female animation libraries |
| `css_urban`, `css_gign`, `css_gsg9`, `css_sas`, `css_phoenix`, `css_leet`, `css_guerilla`, `css_arctic` | Installed Counter-Strike: Source/Garry's Mod player assets |

Valve created the original game characters and assets. Gordon's community port
is the creator-distributed archive `Gordon Freeman`, downloaded from Gamebanana
file 1819930 (published archive MD5 `ddfabbc93212a2eb1a2b6927522893f4`). Asset
origins and final SHA-256 hashes are recorded per file in `asset_manifest.json`.
No CS:GO assets are included in this roster.

The three Portal models are rebuilt on the shared HL2DM female animation rig.
Their skin weights are rebound using anatomical body axes and physical joint
directions. Raw authoring bone axes differ between Portal and ValveBiped, so
matching complete bone frames would twist the torso and limbs. Helper bones
such as springs and twist joints share their mapped ancestor's transform;
hands use physical palm frames. The original first-person weapon models are
separate from these player bodies.

Each rebuilt model declares the four IK chains required by the included
animation library, in the same numeric order. Omitting the main-model chain
table causes included autoplay locks to reference invalid chains. Articulated
ragdoll physics uses 15 convex bodies and the stock female player's joint limits.

Every body includes the multiplayer animation library first and a matching
private neutral locomotion library second. The latter exposes eight unarmed
movement activities. NPC automatic head/spine layers and duplicate autoplay
locks are disabled, preserving the multiplayer weapon poses. All 2,172 actual
MDL/ANI IK rules in the two neutral libraries have valid chain indices; their
four named chains and bind poses match the multiplayer rigs.

The legacy gasmask citizen contained eye-coded meshes without an eyeball table,
and its archived hardware skinning batches rendered only the head. Its complete
weighted geometry is rebuilt through modern studiomdl using the male rig. Its
original eye centers, iris maps and projection settings are recovered from the
2003 source in a material namespace private to this player. Original
mounted NPC materials are not replaced.
The unused original `materials/models/citizen_17/*.vmt` import copies are excluded
from the player package. Their textures remain available to the private player
materials. The cleanup checks compiled model use, preserves tracked source
files, and records the exact omitted paths.

Facial flexes are not rebuilt for `chell`, `chell_2010`, or `mel`. Their original
eyeball centers, iris scales, and projection axes are reconstructed as real eye
meshes, so irises use the engine's animated gaze. Chell and Mel use the original
materials' authored `Eyes_dx8` sclera/iris maps with the current `Eyes` shader;
this avoids the silver iris rings from the non-raytraced `EyeRefract` path.
July 2010 Chell retains her raytraced `EyeRefract` material. Their body, head, and finger
weights are retained and animated. Spring,
ponytail, and twist helpers follow the relevant limb/head without separate
procedural motion. The July 2010 source lacks its `lambert1` material; its torso
texture supplies that small fallback surface.

The compiler-only `player_character_work/assets/gameinfo.txt` is deliberately
excluded from both source overlays. It must not replace the game's search paths.
The overlay manifests cover character files only; weapon delivery is separate.

Validation checks compiled model/geometry/physics checksums, bone weights,
triangle indices, material/texture closure, included activity coverage, all IK
chain contracts and eye-mesh references.

Native verification passed for all 16 selectable bodies and their neutral idle
animations. Chell's run, crouch and jump were exercised in-game. Final close-up
screenshots confirm visible pupils on Chell, July 2010 Chell, Mel and the gasmask
citizen; the masked citizen's complete body renders. The three rebuilt Portal
ragdolls were spawned and inspected in-game. Evidence and its hashes are in
`native_character_verification.json` and `images/`.
