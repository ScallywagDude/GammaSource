# Experimental multiplayer map sources

`mp_modern_test.vmf` provides eight `info_player_deathmatch` spawns and five
ordinary pickups. Compile it to BSP with the matching 2003-era map tools and
copy the BSP to the game `maps/` folder. This is not a functional multiplayer
mode by itself. Multiplayer game rules exist in the original source tree,
but builds, client/server synchronization, and game behavior are untested.

Copy a server preset from `modernize/configs/` into your mod `cfg/` folder;
`sv_lan 1` is deliberately used for first-pass local testing. To enable true
multiplayer, the executable needs to start with multiplayer/deathmatch game
state, load the `multiplay_gamerules` class, and connect multiple clients.
No public server exposure is recommended until security and stability testing.
The example cfg enables optional player map votes: callvote map mp_modern_test,
vote yes/no, and votestatus; public servers default to mp_mapvote 0.

