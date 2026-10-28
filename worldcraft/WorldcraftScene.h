//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef WORLDCRAFTSCENE_H
#define WORLDCRAFTSCENE_H
#ifdef _WIN32
#pragma once
#endif


class CChoreoScene;


// Load the specified vcd file.
CChoreoScene* WorldcraftLoadScene( const char *pFilename );

// Load the VCD file and get the first sound in it.
bool GetFirstSoundInScene( const char *pSceneFilename, char *pSoundName, int soundNameLen );


#endif // HAMMERSCENE_H
