//========= GammaSource ============//
//
// Native Vulkan device used by shaderapivk (kept free of Source headers).
//
//=============================================================================//
#ifndef VKDEVICE_H
#define VKDEVICE_H
#pragma once

bool Vk_Init( void *hWnd, int nWidth, int nHeight );
void Vk_Shutdown();
bool Vk_IsActive();
void Vk_SetClearColor( float r, float g, float b, float a );
void Vk_Clear( bool bColor );
void Vk_Present();
void Vk_GetBackBufferSize( int &nWidth, int &nHeight );
void Vk_GetWindowSize( int &nWidth, int &nHeight );
void Vk_GetDesktopSize( int &nWidth, int &nHeight );
const char *Vk_AdapterName();

#endif // VKDEVICE_H
