//========= GammaSource ============//
//
// Native Vulkan device used by shaderapivk (kept free of Source headers).
// Same functions as the DX11 backend's dx11device.h, so the shared shader API code
// (generated from shaderapidx11) drives either one.
//
//=============================================================================//
#ifndef VKDEVICE_H
#define VKDEVICE_H
#pragma once

bool Vk_Init( void *hWnd, int nWidth, int nHeight );
bool Vk_Resize( int nWidth, int nHeight );
void Vk_Shutdown();
bool Vk_IsActive();
void Vk_SetClearColor( float r, float g, float b, float a );
void Vk_Present();
void Vk_GetBackBufferSize( int &nWidth, int &nHeight );
void Vk_GetWindowSize( int &nWidth, int &nHeight );
void Vk_GetDesktopSize( int &nWidth, int &nHeight );
const char *Vk_AdapterName();
void Vk_DebugLog( const char *pszMsg, int n );

#endif // VKDEVICE_H
