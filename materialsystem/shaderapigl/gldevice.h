//========= GammaSource ============//
//
// Native OpenGL device used by shaderapigl (kept free of Source headers).
// Same functions as the DX11 backend's dx11device.h, so the shared shader API code
// (generated from shaderapidx11) drives either one.
//
//=============================================================================//
#ifndef GLDEVICE_H
#define GLDEVICE_H
#pragma once

bool Gl_Init( void *hWnd, int nWidth, int nHeight );
bool Gl_Resize( int nWidth, int nHeight );
void Gl_Shutdown();
bool Gl_IsActive();
void Gl_SetClearColor( float r, float g, float b, float a );
void Gl_Present();
void Gl_GetBackBufferSize( int &nWidth, int &nHeight );
void Gl_GetWindowSize( int &nWidth, int &nHeight );
void Gl_GetDesktopSize( int &nWidth, int &nHeight );
const char *Gl_AdapterName();
void Gl_DebugLog( const char *pszMsg, int n );

#endif // GLDEVICE_H
