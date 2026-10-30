//========= GammaSource ============//
//
// Native Direct3D 12 device used by shaderapidx12 (kept free of Source headers).
// Same functions as the DX11 backend's dx11device.h, so the shared shader API code
// (generated from shaderapidx11) drives either one.
//
//=============================================================================//
#ifndef DX12DEVICE_H
#define DX12DEVICE_H
#pragma once

bool Dx12_Init( void *hWnd, int nWidth, int nHeight );
bool Dx12_Resize( int nWidth, int nHeight );
void Dx12_Shutdown();
bool Dx12_IsActive();
void Dx12_SetClearColor( float r, float g, float b, float a );
void Dx12_Present();
void Dx12_GetBackBufferSize( int &nWidth, int &nHeight );
void Dx12_GetWindowSize( int &nWidth, int &nHeight );
void Dx12_GetDesktopSize( int &nWidth, int &nHeight );
const char *Dx12_AdapterName();
void Dx12_DebugLog( const char *pszMsg, int n );

#endif // DX12DEVICE_H
