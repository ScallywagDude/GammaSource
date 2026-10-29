//========= GammaSource ============//
//
// Native Direct3D 11 device used by shaderapidx11 (kept free of Source headers).
//
//=============================================================================//
#ifndef DX11DEVICE_H
#define DX11DEVICE_H
#pragma once

bool Dx11_Init( void *hWnd, int nWidth, int nHeight );
bool Dx11_Resize( int nWidth, int nHeight );
void Dx11_Shutdown();
bool Dx11_IsActive();
void Dx11_SetClearColor( float r, float g, float b, float a );
void Dx11_Clear( bool bColor );
void Dx11_Present();
void Dx11_GetBackBufferSize( int &nWidth, int &nHeight );
void Dx11_GetWindowSize( int &nWidth, int &nHeight );
const char *Dx11_AdapterName();
void Dx11_GetDesktopSize( int &nWidth, int &nHeight );

#endif // DX11DEVICE_H
