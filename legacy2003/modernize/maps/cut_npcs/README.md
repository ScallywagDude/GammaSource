# Cut NPC disposable map sources

Hammer Editor VMF inputs, one prototype entity per map. Compile with your
working VBSP/VVIS/VRAD tools after configuring the proper HL2 game directory.
The maps are test inputs, NOT runnable BSPs. Some prototypes can crash during
spawn, have missing models/sounds, or lack AI. Do not load on live servers.

These maps attempt to force level-start precaching instead of enabling
`npc_allow_unprecached_create`; only the registered Debug HL2 candidates are
included. The disabled Conscript code is not represented. NPC entity models,
AI behavior and eventual multiplayer replication all require testing.

The generated VMFs use one default Source 2003-era brick floor material and a
simple nodraw room. Update the texture or skybox if your content differs.
