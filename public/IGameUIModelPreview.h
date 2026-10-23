#ifndef IGAMEUI_MODEL_PREVIEW_H
#define IGAMEUI_MODEL_PREVIEW_H
#ifdef _WIN32
#pragma once
#endif

// Separate optional interface: the 2003 IVEngineClient and IGameUIFuncs
// vtables remain unchanged. Render is called only inside Begin3DPaint.
#define GAMEUI_MODEL_PREVIEW_INTERFACE_VERSION "GameUIModelPreview001"
class IGameUIModelPreview
{
public:
    virtual bool IsModelAvailable( const char *modelPath ) = 0;
    virtual bool SetModel( const char *modelPath ) = 0;
    virtual void ClearModel() = 0;
    virtual bool Render( int width, int height, float yaw, float elapsedSeconds ) = 0;
    virtual const char *GetStatusText() = 0;
};
#endif
