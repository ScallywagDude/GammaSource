//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: interface to asynchronous lighting preview creator
//
//===========================================================================//

#ifndef LPREVIEW_THREAD_H
#define LPREVIEW_THREAD_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/threadtools.h"
#include "bitmap/bitmap.h"
#include "bitmap/float_bm.h"
#include "tier1/utlvector.h"
#include "mathlib/lightdesc.h"





class CLightingPreviewLightDescription : public LightDesc_t
{
public:
	int m_nObjectID;
	class CIncrementalLightInfo *m_pIncrementalInfo;
	void Init( int obj_id )
	{
		m_pIncrementalInfo = NULL;
		m_nObjectID = obj_id;
	}
};

enum WorldcraftToLightingPreviewMessageType
{
	// messages from hammer to preview task
	LPREVIEW_MSG_STOP,									// no lighting previews open - stop working
	LPREVIEW_MSG_EXIT,										// we're exiting program - shut down
	LPREVIEW_MSG_GEOM_DATA,									  // we have new shadow geometry data
	LPREVIEW_MSG_G_BUFFERS,							 // we have new g buffer data from the renderer
	LPREVIEW_MSG_LIGHT_DATA,								// new light data in m_pLightList
};

enum LightingPreviewToWorldcraftMessageType
{
	// messages from preview task to hammer
	LPREVIEW_MSG_DISPLAY_RESULT,							// we have a result image
};


struct MessageToLPreview
{
	WorldcraftToLightingPreviewMessageType m_MsgType;

	MessageToLPreview( WorldcraftToLightingPreviewMessageType mtype)
	{
		m_MsgType = mtype;
	}
	MessageToLPreview( void)
	{
	}

	// this structure uses a fat format for the args instead of separate classes for each
	// message. the messages are small anyway, since pointers are used for anything of size.
	FloatBitMap_t *m_pDefferedRenderingBMs[4];				// if LPREVIEW_MSG_G_BUFFERS
	CUtlVector<CLightingPreviewLightDescription> *m_pLightList;	// if LPREVIEW_MSG_LIGHT_DATA
	Vector m_EyePosition;									// for LPREVIEW_MSG_LIGHT_DATA & G_BUFFERS
	CUtlVector<Vector> *m_pShadowTriangleList;				// for LPREVIEW_MSG_GEOM_DATA
	int m_nBitmapGenerationCounter;							// for LPREVIEW_MSG_G_BUFFERS

};

struct MessageFromLPreview
{
	LightingPreviewToWorldcraftMessageType m_MsgType;
	Bitmap_t *m_pBitmapToDisplay;							// for LPREVIEW_MSG_DISPLAY_RESULT
	int m_nBitmapGenerationCounter;							// for LPREVIEW_MSG_DISPLAY_RESULT
	MessageFromLPreview( LightingPreviewToWorldcraftMessageType msgtype )
	{
		m_MsgType = msgtype;
	}
	MessageFromLPreview( void )
	{
	}

};

extern CMessageQueue<MessageToLPreview> g_WorldcraftToLPreviewMsgQueue;
extern CMessageQueue<MessageFromLPreview> g_LPreviewToWorldcraftMsgQueue;
extern ThreadHandle_t g_LPreviewThread;

extern CInterlockedInt n_gbufs_queued;
extern CInterlockedInt n_result_bms_queued;

extern Bitmap_t *g_pLPreviewOutputBitmap;

// the lighting preview thread entry point

uintp LightingPreviewThreadFN( void *thread_start_arg );

// the lighting preview handler. call often
void HandleLightingPreview( void );

#endif

