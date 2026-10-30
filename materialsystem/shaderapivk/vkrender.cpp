//========= GammaSource ============//
//
// Native Vulkan renderer for shaderapivk (kept free of Source headers): instance, device,
// swap chain, textures, render targets, pipelines and the textured, vertex-coloured,
// lightmapped draw path the materials' passes go through. It implements the same
// functions as the DX11 backend (vkdevice.h, vkrender.h).
//
// vulkan-1.dll is loaded at run time, so the game still starts on machines without
// Vulkan (the launcher then falls back to DirectX 9).
//
// Frames: the game draws into an offscreen back buffer that is copied into the swap
// chain image at Present. Render targets and the back buffer stay in GENERAL layout so
// they can be drawn to, sampled and copied without tracking. Uploads and copies are
// recorded into the frame's command buffer outside render passes.
//
//=============================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include "vkdevice.h"
#include "vkrender.h"
#include "vkshaders.h"

#pragma comment( lib, "user32.lib" )

//-----------------------------------------------------------------------------
// Loader
//-----------------------------------------------------------------------------
static HMODULE s_hVulkan;
static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr_;
#define VK_INSTANCE_FUNCS( X ) \
	X( vkDestroyInstance ) X( vkEnumeratePhysicalDevices ) X( vkGetPhysicalDeviceProperties ) \
	X( vkGetPhysicalDeviceQueueFamilyProperties ) X( vkGetPhysicalDeviceSurfaceSupportKHR ) \
	X( vkGetPhysicalDeviceSurfaceCapabilitiesKHR ) X( vkGetPhysicalDeviceSurfaceFormatsKHR ) \
	X( vkCreateWin32SurfaceKHR ) X( vkDestroySurfaceKHR ) X( vkCreateDevice ) X( vkGetDeviceProcAddr ) \
	X( vkGetPhysicalDeviceMemoryProperties ) X( vkGetPhysicalDeviceFormatProperties )
#define VK_DEVICE_FUNCS( X ) \
	X( vkDestroyDevice ) X( vkGetDeviceQueue ) X( vkDeviceWaitIdle ) X( vkCreateSwapchainKHR ) X( vkDestroySwapchainKHR ) \
	X( vkGetSwapchainImagesKHR ) X( vkAcquireNextImageKHR ) X( vkQueuePresentKHR ) X( vkCreateCommandPool ) \
	X( vkDestroyCommandPool ) X( vkAllocateCommandBuffers ) X( vkBeginCommandBuffer ) X( vkEndCommandBuffer ) \
	X( vkResetCommandBuffer ) X( vkCmdPipelineBarrier ) X( vkQueueSubmit ) X( vkCreateSemaphore ) X( vkDestroySemaphore ) \
	X( vkCreateFence ) X( vkDestroyFence ) X( vkWaitForFences ) X( vkResetFences ) X( vkAllocateMemory ) X( vkFreeMemory ) \
	X( vkMapMemory ) X( vkCreateBuffer ) X( vkDestroyBuffer ) X( vkGetBufferMemoryRequirements ) X( vkBindBufferMemory ) \
	X( vkCreateImage ) X( vkDestroyImage ) X( vkGetImageMemoryRequirements ) X( vkBindImageMemory ) X( vkCreateImageView ) \
	X( vkDestroyImageView ) X( vkCreateSampler ) X( vkDestroySampler ) X( vkCreateRenderPass ) X( vkDestroyRenderPass ) \
	X( vkCreateFramebuffer ) X( vkDestroyFramebuffer ) X( vkCreateShaderModule ) X( vkDestroyShaderModule ) \
	X( vkCreatePipelineLayout ) X( vkDestroyPipelineLayout ) X( vkCreateGraphicsPipelines ) X( vkDestroyPipeline ) \
	X( vkCreateDescriptorSetLayout ) X( vkDestroyDescriptorSetLayout ) X( vkCreateDescriptorPool ) X( vkDestroyDescriptorPool ) \
	X( vkResetDescriptorPool ) X( vkAllocateDescriptorSets ) X( vkUpdateDescriptorSets ) X( vkCmdBeginRenderPass ) \
	X( vkCmdEndRenderPass ) X( vkCmdBindPipeline ) X( vkCmdBindDescriptorSets ) X( vkCmdBindVertexBuffers ) \
	X( vkCmdBindIndexBuffer ) X( vkCmdDraw ) X( vkCmdDrawIndexed ) X( vkCmdPushConstants ) X( vkCmdSetViewport ) \
	X( vkCmdSetScissor ) X( vkCmdClearAttachments ) X( vkCmdCopyBufferToImage ) X( vkCmdCopyImage ) X( vkCmdBlitImage )
#define VK_DECLARE( name ) static PFN_##name name##_;
VK_DECLARE( vkCreateInstance )
VK_INSTANCE_FUNCS( VK_DECLARE )
VK_DEVICE_FUNCS( VK_DECLARE )

//-----------------------------------------------------------------------------
// Device state
//-----------------------------------------------------------------------------
enum { FRAMES = 2, STREAM_SIZE = 16 * 1024 * 1024, BLOCK_SIZE = 64 * 1024 * 1024 };

static VkInstance s_Instance;
static VkPhysicalDevice s_PhysicalDevice;
static VkPhysicalDeviceMemoryProperties s_MemoryProps;
static VkSurfaceKHR s_Surface;
static VkDevice s_Device;
static VkQueue s_Queue;
static uint32_t s_nQueueFamily;
static VkSwapchainKHR s_Swapchain;
static VkImage s_SwapImages[8];
static uint32_t s_nSwapImages;
static int s_nSwapWidth, s_nSwapHeight;
static VkFormat s_DepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
static VkCommandPool s_CommandPool;
static VkCommandBuffer s_Commands[FRAMES];
static VkSemaphore s_ImageReady[FRAMES], s_RenderDone[FRAMES];
static VkFence s_Fences[FRAMES];
static int s_nFrame;			// current slot
static bool s_bRecording;		// the slot's command buffer is open
static HWND s_hWnd;
static float s_ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static char s_szAdapterName[256] = "Vulkan";

void Vk_DebugLog( const char *pszMsg, int n )
{
	static char s_szFile[MAX_PATH];
	static const char *s_pszFile = GetEnvironmentVariableA( "GAMMA_VK_LOG", s_szFile, MAX_PATH ) ? s_szFile : NULL;
	if ( !s_pszFile )
		return;
	FILE *f = fopen( s_pszFile, "a" );
	if ( f ) { fprintf( f, "%s %d\n", pszMsg, n ); fclose( f ); }
}

//-----------------------------------------------------------------------------
// Memory: big blocks per memory type, carved up first-fit
//-----------------------------------------------------------------------------
struct Allocation_t
{
	VkDeviceMemory m_Memory;
	VkDeviceSize m_nOffset, m_nSize;
	int m_nBlock;			// -1: dedicated allocation
	unsigned char *m_pMapped;
};

struct Block_t
{
	VkDeviceMemory m_Memory;
	uint32_t m_nType;
	VkDeviceSize m_nSize;
	unsigned char *m_pMapped;
	std::vector< std::pair< VkDeviceSize, VkDeviceSize > > m_Free;	// offset, size
};
static std::vector< Block_t > s_Blocks;

static int FindMemoryType( uint32_t nBits, VkMemoryPropertyFlags flags )
{
	for ( uint32_t i = 0; i < s_MemoryProps.memoryTypeCount; ++i )
		if ( ( nBits & ( 1u << i ) ) && ( s_MemoryProps.memoryTypes[i].propertyFlags & flags ) == flags )
			return (int)i;
	return -1;
}

static bool Allocate( const VkMemoryRequirements &req, VkMemoryPropertyFlags flags, Allocation_t &out )
{
	memset( &out, 0, sizeof( out ) );
	int nType = FindMemoryType( req.memoryTypeBits, flags );
	if ( nType < 0 && ( flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) )
		nType = FindMemoryType( req.memoryTypeBits, flags & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( nType < 0 )
		return false;
	bool bHost = ( flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) != 0;
	if ( req.size > BLOCK_SIZE / 2 )
	{
		VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, (uint32_t)nType };
		if ( vkAllocateMemory_( s_Device, &ai, NULL, &out.m_Memory ) != VK_SUCCESS )
			return false;
		out.m_nSize = req.size;
		out.m_nBlock = -1;
		if ( bHost )
			vkMapMemory_( s_Device, out.m_Memory, 0, VK_WHOLE_SIZE, 0, (void **)&out.m_pMapped );
		return true;
	}
	for ( int pass = 0; pass < 2; ++pass )
	{
		for ( size_t b = 0; b < s_Blocks.size(); ++b )
		{
			Block_t &block = s_Blocks[b];
			if ( block.m_nType != (uint32_t)nType )
				continue;
			for ( size_t f = 0; f < block.m_Free.size(); ++f )
			{
				VkDeviceSize nStart = ( block.m_Free[f].first + req.alignment - 1 ) / req.alignment * req.alignment;
				VkDeviceSize nEnd = block.m_Free[f].first + block.m_Free[f].second;
				if ( nStart + req.size > nEnd )
					continue;
				// split the free range around the allocation
				VkDeviceSize nBefore = nStart - block.m_Free[f].first;
				VkDeviceSize nAfter = nEnd - ( nStart + req.size );
				VkDeviceSize nRangeStart = block.m_Free[f].first;
				block.m_Free.erase( block.m_Free.begin() + f );
				if ( nAfter ) block.m_Free.push_back( std::make_pair( nStart + req.size, nAfter ) );
				if ( nBefore ) block.m_Free.push_back( std::make_pair( nRangeStart, nBefore ) );
				out.m_Memory = block.m_Memory;
				out.m_nOffset = nStart;
				out.m_nSize = req.size;
				out.m_nBlock = (int)b;
				out.m_pMapped = block.m_pMapped ? block.m_pMapped + nStart : NULL;
				return true;
			}
		}
		if ( pass == 1 )
			break;
		Block_t block;
		VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, BLOCK_SIZE, (uint32_t)nType };
		if ( vkAllocateMemory_( s_Device, &ai, NULL, &block.m_Memory ) != VK_SUCCESS )
			return false;
		block.m_nType = nType;
		block.m_nSize = BLOCK_SIZE;
		block.m_pMapped = NULL;
		if ( bHost )
			vkMapMemory_( s_Device, block.m_Memory, 0, VK_WHOLE_SIZE, 0, (void **)&block.m_pMapped );
		block.m_Free.push_back( std::make_pair( (VkDeviceSize)0, (VkDeviceSize)BLOCK_SIZE ) );
		s_Blocks.push_back( block );
	}
	return false;
}

static void Free( Allocation_t &a )
{
	if ( !a.m_Memory )
		return;
	if ( a.m_nBlock < 0 )
	{
		vkFreeMemory_( s_Device, a.m_Memory, NULL );
	}
	else if ( a.m_nBlock < (int)s_Blocks.size() )
	{
		Block_t &block = s_Blocks[a.m_nBlock];
		VkDeviceSize nStart = a.m_nOffset, nEnd = a.m_nOffset + a.m_nSize;
		// merge with neighbours
		for ( size_t f = 0; f < block.m_Free.size(); )
		{
			VkDeviceSize fs = block.m_Free[f].first, fe = fs + block.m_Free[f].second;
			if ( fe == nStart || fs == nEnd )
			{
				if ( fe == nStart ) nStart = fs; else nEnd = fe;
				block.m_Free.erase( block.m_Free.begin() + f );
				f = 0;
				continue;
			}
			++f;
		}
		block.m_Free.push_back( std::make_pair( nStart, nEnd - nStart ) );
	}
	memset( &a, 0, sizeof( a ) );
}

//-----------------------------------------------------------------------------
// Deferred destruction: freed once the frame that used the object has finished
//-----------------------------------------------------------------------------
struct Garbage_t
{
	VkImage m_Image;
	VkImageView m_View;
	VkBuffer m_Buffer;
	VkFramebuffer m_Framebuffer;
	Allocation_t m_Memory;
};
static std::vector< Garbage_t > s_Garbage[FRAMES];

static void CollectGarbage( int nSlot )
{
	for ( size_t i = 0; i < s_Garbage[nSlot].size(); ++i )
	{
		Garbage_t &g = s_Garbage[nSlot][i];
		if ( g.m_Framebuffer ) vkDestroyFramebuffer_( s_Device, g.m_Framebuffer, NULL );
		if ( g.m_View ) vkDestroyImageView_( s_Device, g.m_View, NULL );
		if ( g.m_Image ) vkDestroyImage_( s_Device, g.m_Image, NULL );
		if ( g.m_Buffer ) vkDestroyBuffer_( s_Device, g.m_Buffer, NULL );
		Free( g.m_Memory );
	}
	s_Garbage[nSlot].clear();
}

//-----------------------------------------------------------------------------
// Per-frame streaming memory (vertices/indices used once, upload staging)
//-----------------------------------------------------------------------------
struct StreamBuffer_t
{
	VkBuffer m_Buffer;
	Allocation_t m_Memory;
	VkDeviceSize m_nSize, m_nUsed;
};
static std::vector< StreamBuffer_t > s_Stream[FRAMES];

static bool CreateBuffer( VkDeviceSize nSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags, VkBuffer &buffer, Allocation_t &memory )
{
	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = nSize;
	bci.usage = usage;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( vkCreateBuffer_( s_Device, &bci, NULL, &buffer ) != VK_SUCCESS )
		return false;
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements_( s_Device, buffer, &req );
	if ( !Allocate( req, flags, memory ) )
	{
		vkDestroyBuffer_( s_Device, buffer, NULL );
		buffer = VK_NULL_HANDLE;
		return false;
	}
	vkBindBufferMemory_( s_Device, buffer, memory.m_Memory, memory.m_nOffset );
	return true;
}

// host-visible memory for this frame; valid until the slot comes around again
static unsigned char *StreamAlloc( VkDeviceSize nBytes, VkBuffer &buffer, VkDeviceSize &nOffset )
{
	std::vector< StreamBuffer_t > &stream = s_Stream[s_nFrame];
	for ( size_t i = 0; i < stream.size(); ++i )
	{
		StreamBuffer_t &sb = stream[i];
		VkDeviceSize nStart = ( sb.m_nUsed + 255 ) & ~(VkDeviceSize)255;
		if ( nStart + nBytes <= sb.m_nSize )
		{
			sb.m_nUsed = nStart + nBytes;
			buffer = sb.m_Buffer;
			nOffset = nStart;
			return sb.m_Memory.m_pMapped + nStart;
		}
	}
	StreamBuffer_t sb = {};
	sb.m_nSize = nBytes > STREAM_SIZE ? ( nBytes + 255 ) & ~(VkDeviceSize)255 : STREAM_SIZE;
	if ( !CreateBuffer( sb.m_nSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sb.m_Buffer, sb.m_Memory ) || !sb.m_Memory.m_pMapped )
		return NULL;
	sb.m_nUsed = nBytes;
	stream.push_back( sb );
	buffer = sb.m_Buffer;
	nOffset = 0;
	return sb.m_Memory.m_pMapped;
}

// big frames (level load) would grow the stream without bound: past this, wait for the GPU
static VkDeviceSize StreamUsed()
{
	VkDeviceSize n = 0;
	for ( size_t i = 0; i < s_Stream[s_nFrame].size(); ++i )
		n += s_Stream[s_nFrame][i].m_nUsed;
	return n;
}

//-----------------------------------------------------------------------------
// Textures and render targets
//-----------------------------------------------------------------------------
struct Texture_t
{
	VkImage m_Image;
	VkImageView m_View;
	Allocation_t m_Memory;
	VkImageLayout m_Layout;
	int m_nWidth, m_nHeight, m_nMips;
	bool m_bClampS, m_bClampT;
	bool m_bUsed;
	bool m_bHasData;
	bool m_bRenderTarget;
	unsigned char *m_pPending[16];	// mip levels given before the device existed
	unsigned char *m_pShadow;		// see VkR_LockShadow
	// render targets
	VkImage m_Depth;
	VkImageView m_DepthView;
	Allocation_t m_DepthMemory;
	VkFramebuffer m_Framebuffer;
};
static std::vector< Texture_t > s_Textures;	// handle = index + 1
static Texture_t s_BackBuffer;			// what the game draws into; copied to the swap chain
static int s_hWhite;
static int s_hBound;
static int s_hLightmap;
static int s_hRenderTarget;				// 0: the back buffer

static Texture_t *GetTexture( int hTexture )
{
	if ( hTexture <= 0 || hTexture > (int)s_Textures.size() || !s_Textures[hTexture - 1].m_bUsed )
		return NULL;
	return &s_Textures[hTexture - 1];
}

static int MipSize( int n, int nLevel )
{
	n >>= nLevel;
	return n > 0 ? n : 1;
}

static VkRenderPass s_RenderPass;
static VkCommandBuffer s_Cmd;			// the open command buffer (s_Commands[s_nFrame])
static Texture_t *s_pPassTarget;		// target of the open render pass, or NULL

static void BeginRecording();
static void EndPass();

static void ImageBarrier( VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout )
{
	VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	b.oldLayout = oldLayout;
	b.newLayout = newLayout;
	b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = image;
	b.subresourceRange.aspectMask = aspect;
	b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	b.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier_( s_Cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &b );
}

static bool CreateImage( int nWidth, int nHeight, int nMips, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
	VkImage &image, VkImageView &view, Allocation_t &memory )
{
	VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = format;
	ici.extent.width = nWidth; ici.extent.height = nHeight; ici.extent.depth = 1;
	ici.mipLevels = nMips;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = usage;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( vkCreateImage_( s_Device, &ici, NULL, &image ) != VK_SUCCESS )
		return false;
	VkMemoryRequirements req;
	vkGetImageMemoryRequirements_( s_Device, image, &req );
	if ( !Allocate( req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory ) )
	{
		vkDestroyImage_( s_Device, image, NULL ); image = VK_NULL_HANDLE;
		return false;
	}
	vkBindImageMemory_( s_Device, image, memory.m_Memory, memory.m_nOffset );
	VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	vci.image = image;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = format;
	vci.subresourceRange.aspectMask = aspect;
	vci.subresourceRange.levelCount = nMips;
	vci.subresourceRange.layerCount = 1;
	return vkCreateImageView_( s_Device, &vci, NULL, &view ) == VK_SUCCESS;
}

static void UploadRegion( Texture_t *pTex, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch );

// creates the GPU image (and for render targets the depth buffer and framebuffer)
static bool EnsureResource( Texture_t *pTex )
{
	if ( pTex->m_Image )
		return true;
	if ( !s_Device )
		return false;
	VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if ( pTex->m_bRenderTarget )
		usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( !CreateImage( pTex->m_nWidth, pTex->m_nHeight, pTex->m_nMips, VK_FORMAT_B8G8R8A8_UNORM, usage, VK_IMAGE_ASPECT_COLOR_BIT,
		pTex->m_Image, pTex->m_View, pTex->m_Memory ) )
		return false;
	pTex->m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( pTex->m_bRenderTarget )
	{
		if ( !CreateImage( pTex->m_nWidth, pTex->m_nHeight, 1, s_DepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_IMAGE_ASPECT_DEPTH_BIT, pTex->m_Depth, pTex->m_DepthView, pTex->m_DepthMemory ) )
			return false;
		VkImageView views[2] = { pTex->m_View, pTex->m_DepthView };
		VkFramebufferCreateInfo fci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fci.renderPass = s_RenderPass;
		fci.attachmentCount = 2;
		fci.pAttachments = views;
		fci.width = pTex->m_nWidth; fci.height = pTex->m_nHeight; fci.layers = 1;
		if ( vkCreateFramebuffer_( s_Device, &fci, NULL, &pTex->m_Framebuffer ) != VK_SUCCESS )
			return false;
		// render targets live in GENERAL: drawable, sampleable and copyable
		BeginRecording();
		EndPass();
		ImageBarrier( pTex->m_Image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );
		VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( s_DepthFormat != VK_FORMAT_D32_SFLOAT ) depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		ImageBarrier( pTex->m_Depth, depthAspect, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
		pTex->m_Layout = VK_IMAGE_LAYOUT_GENERAL;
		pTex->m_bHasData = true;
	}
	// data that arrived before the device existed
	for ( int i = 0; i < 16 && i < pTex->m_nMips; ++i )
	{
		if ( !pTex->m_pPending[i] )
			continue;
		UploadRegion( pTex, i, 0, 0, MipSize( pTex->m_nWidth, i ), MipSize( pTex->m_nHeight, i ), pTex->m_pPending[i], MipSize( pTex->m_nWidth, i ) * 4 );
		free( pTex->m_pPending[i] );
		pTex->m_pPending[i] = NULL;
	}
	return true;
}

static void DestroyTexture( Texture_t *pTex, bool bDeferred )
{
	Garbage_t g = {};
	g.m_Image = pTex->m_Image; g.m_View = pTex->m_View; g.m_Memory = pTex->m_Memory;
	Garbage_t d = {};
	d.m_Image = pTex->m_Depth; d.m_View = pTex->m_DepthView; d.m_Memory = pTex->m_DepthMemory; d.m_Framebuffer = pTex->m_Framebuffer;
	if ( bDeferred )
	{
		s_Garbage[s_nFrame].push_back( g );
		s_Garbage[s_nFrame].push_back( d );
	}
	else
	{
		if ( d.m_Framebuffer ) vkDestroyFramebuffer_( s_Device, d.m_Framebuffer, NULL );
		if ( d.m_View ) vkDestroyImageView_( s_Device, d.m_View, NULL );
		if ( d.m_Image ) vkDestroyImage_( s_Device, d.m_Image, NULL );
		Free( d.m_Memory );
		if ( g.m_View ) vkDestroyImageView_( s_Device, g.m_View, NULL );
		if ( g.m_Image ) vkDestroyImage_( s_Device, g.m_Image, NULL );
		Free( g.m_Memory );
	}
	for ( int i = 0; i < 16; ++i ) free( pTex->m_pPending[i] );
	free( pTex->m_pShadow );
	memset( pTex, 0, sizeof( *pTex ) );
}

//-----------------------------------------------------------------------------
// Pipelines
//-----------------------------------------------------------------------------
struct PushConstants_t
{
	float m_WVP[16];
	float m_AlphaRef;
	float m_AlphaTest;
	float m_Lightmap;
	float m_Pad;
};

static VkDescriptorSetLayout s_SetLayout;
static VkPipelineLayout s_PipelineLayout;
static VkShaderModule s_VS, s_FS;
static VkSampler s_Samplers[4];		// bit 0 clamp S, bit 1 clamp T
static std::map< unsigned int, VkPipeline > s_Pipelines;
static std::vector< VkDescriptorPool > s_DescriptorPools[FRAMES];
static size_t s_nDescriptorPool[FRAMES];
static VkRState_t s_State;
int g_nVkDraws, g_nVkMeshes, g_nVkPasses, g_nVkEmptyMeshes;
static float s_WVP[16];
static VkViewport s_Viewport;
static bool s_bViewportSet;

static const VkBlendFactor s_BlendMap[] =
{
	VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR
};
static const VkCompareOp s_DepthMap[] =
{
	VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL,
	VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS
};
static const VkPrimitiveTopology s_Topology[] =
{
	VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP
};

static VkPipeline GetPipeline( VkRPrimitive_t type )
{
	int nSrc = ( s_State.m_nSrcBlend >= 0 && s_State.m_nSrcBlend <= 10 ) ? s_State.m_nSrcBlend : 1;
	int nDst = ( s_State.m_nDstBlend >= 0 && s_State.m_nDstBlend <= 10 ) ? s_State.m_nDstBlend : 0;
	int nDepthFunc = ( s_State.m_nDepthFunc >= 0 && s_State.m_nDepthFunc <= 7 ) ? s_State.m_nDepthFunc : 3;
	unsigned int nKey = ( s_State.m_bBlend ? 1 : 0 ) | ( nSrc << 1 ) | ( nDst << 5 ) | ( s_State.m_bDepthTest ? 1 << 9 : 0 ) |
		( s_State.m_bDepthWrite ? 1 << 10 : 0 ) | ( nDepthFunc << 11 ) | ( s_State.m_bCull ? 1 << 14 : 0 ) | ( (unsigned)type << 15 );
	std::map< unsigned int, VkPipeline >::iterator it = s_Pipelines.find( nKey );
	if ( it != s_Pipelines.end() )
		return it->second;

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = s_VS; stages[0].pName = "main";
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = s_FS; stages[1].pName = "main";

	VkVertexInputBindingDescription binding = { 0, sizeof( VkRVertex_t ), VK_VERTEX_INPUT_RATE_VERTEX };
	VkVertexInputAttributeDescription attrs[4] =
	{
		{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
		{ 1, 0, VK_FORMAT_R8G8B8A8_UNORM, 12 },
		{ 2, 0, VK_FORMAT_R32G32_SFLOAT, 16 },
		{ 3, 0, VK_FORMAT_R32G32_SFLOAT, 24 },
	};
	VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &binding;
	vi.vertexAttributeDescriptionCount = 4; vi.pVertexAttributeDescriptions = attrs;
	VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = s_Topology[type];
	VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vp.viewportCount = 1; vp.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = s_State.m_bCull ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	// the viewport is flipped (negative height) to match Direct3D, so D3D's clockwise front faces hold
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = s_State.m_bDepthTest || s_State.m_bDepthWrite;
	ds.depthWriteEnable = s_State.m_bDepthWrite;
	ds.depthCompareOp = s_State.m_bDepthTest ? s_DepthMap[nDepthFunc] : VK_COMPARE_OP_ALWAYS;
	VkPipelineColorBlendAttachmentState cb = {};
	cb.blendEnable = s_State.m_bBlend;
	cb.srcColorBlendFactor = s_BlendMap[nSrc];
	cb.dstColorBlendFactor = s_BlendMap[nDst];
	cb.colorBlendOp = VK_BLEND_OP_ADD;
	cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cb.alphaBlendOp = VK_BLEND_OP_ADD;
	cb.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo cbs = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = 1; cbs.pAttachments = &cb;
	VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dys = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dys.dynamicStateCount = 2; dys.pDynamicStates = dyn;

	VkGraphicsPipelineCreateInfo gpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gpci.stageCount = 2; gpci.pStages = stages;
	gpci.pVertexInputState = &vi; gpci.pInputAssemblyState = &ia; gpci.pViewportState = &vp;
	gpci.pRasterizationState = &rs; gpci.pMultisampleState = &ms; gpci.pDepthStencilState = &ds;
	gpci.pColorBlendState = &cbs; gpci.pDynamicState = &dys;
	gpci.layout = s_PipelineLayout;
	gpci.renderPass = s_RenderPass;
	VkPipeline pipeline = VK_NULL_HANDLE;
	vkCreateGraphicsPipelines_( s_Device, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline );
	s_Pipelines[nKey] = pipeline;
	return pipeline;
}

//-----------------------------------------------------------------------------
// Frames and render passes
//-----------------------------------------------------------------------------
static void BeginRecording()
{
	if ( s_bRecording || !s_Device )
		return;
	vkWaitForFences_( s_Device, 1, &s_Fences[s_nFrame], VK_TRUE, UINT64_MAX );
	CollectGarbage( s_nFrame );
	for ( size_t i = 0; i < s_Stream[s_nFrame].size(); ++i )
		s_Stream[s_nFrame][i].m_nUsed = 0;
	for ( size_t i = 0; i < s_DescriptorPools[s_nFrame].size(); ++i )
		vkResetDescriptorPool_( s_Device, s_DescriptorPools[s_nFrame][i], 0 );
	s_nDescriptorPool[s_nFrame] = 0;
	s_Cmd = s_Commands[s_nFrame];
	vkResetCommandBuffer_( s_Cmd, 0 );
	VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer_( s_Cmd, &begin );
	s_bRecording = true;
}

static void EndPass()
{
	if ( !s_pPassTarget )
		return;
	vkCmdEndRenderPass_( s_Cmd );
	s_pPassTarget = NULL;
	// what was drawn is read by later draws, copies and the present blit
	VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	vkCmdPipelineBarrier_( s_Cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 1, &mb, 0, NULL, 0, NULL );
}

static Texture_t *CurrentTarget()
{
	Texture_t *pTex = GetTexture( s_hRenderTarget );
	return ( pTex && pTex->m_Framebuffer ) ? pTex : &s_BackBuffer;
}

static bool BeginPass()
{
	BeginRecording();
	if ( !s_bRecording )
		return false;
	Texture_t *pTarget = CurrentTarget();
	if ( s_pPassTarget == pTarget )
		return true;
	EndPass();
	if ( !pTarget->m_Framebuffer )
		return false;
	VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rpbi.renderPass = s_RenderPass;
	rpbi.framebuffer = pTarget->m_Framebuffer;
	rpbi.renderArea.extent.width = pTarget->m_nWidth;
	rpbi.renderArea.extent.height = pTarget->m_nHeight;
	vkCmdBeginRenderPass_( s_Cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE );
	s_pPassTarget = pTarget;
	return true;
}

// submit what is recorded and wait for it: used when a frame's streaming memory
// gets large (level loads upload hundreds of megabytes before the first Present)
static void SubmitAndWait()
{
	if ( !s_bRecording )
		return;
	EndPass();
	vkEndCommandBuffer_( s_Cmd );
	vkResetFences_( s_Device, 1, &s_Fences[s_nFrame] );
	VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &s_Cmd;
	vkQueueSubmit_( s_Queue, 1, &submit, s_Fences[s_nFrame] );
	s_bRecording = false;
	BeginRecording();	// waits on the fence and recycles the slot
}

//-----------------------------------------------------------------------------
// Uploads
//-----------------------------------------------------------------------------
static void UploadRegion( Texture_t *pTex, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch )
{
	if ( nWidth <= 0 || nHeight <= 0 )
		return;
	BeginRecording();
	if ( StreamUsed() > 256 * 1024 * 1024 )
		SubmitAndWait();
	VkBuffer buffer; VkDeviceSize nOffset;
	unsigned char *pDst = StreamAlloc( (VkDeviceSize)nWidth * nHeight * 4, buffer, nOffset );
	if ( !pDst )
		return;
	for ( int row = 0; row < nHeight; ++row )
		memcpy( pDst + row * nWidth * 4, pBGRA + row * nPitch, nWidth * 4 );
	EndPass();
	VkImageLayout finalLayout = pTex->m_bRenderTarget ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ImageBarrier( pTex->m_Image, VK_IMAGE_ASPECT_COLOR_BIT, pTex->m_Layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
	VkBufferImageCopy copy = {};
	copy.bufferOffset = nOffset;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.mipLevel = nLevel;
	copy.imageSubresource.layerCount = 1;
	copy.imageOffset.x = x; copy.imageOffset.y = y;
	copy.imageExtent.width = nWidth; copy.imageExtent.height = nHeight; copy.imageExtent.depth = 1;
	vkCmdCopyBufferToImage_( s_Cmd, buffer, pTex->m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy );
	ImageBarrier( pTex->m_Image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, finalLayout );
	pTex->m_Layout = finalLayout;
	pTex->m_bHasData = true;
}

static void KeepPending( Texture_t *pTex, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	if ( nLevel >= 16 )
		return;
	int nLevelW = MipSize( pTex->m_nWidth, nLevel ), nLevelH = MipSize( pTex->m_nHeight, nLevel );
	if ( !pTex->m_pPending[nLevel] )
		pTex->m_pPending[nLevel] = (unsigned char *)calloc( nLevelW * nLevelH, 4 );
	for ( int row = 0; row < nHeight && y + row < nLevelH; ++row )
	{
		int nCopy = nWidth;
		if ( x + nCopy > nLevelW ) nCopy = nLevelW - x;
		if ( nCopy > 0 )
			memcpy( pTex->m_pPending[nLevel] + ( ( y + row ) * nLevelW + x ) * 4, pBGRA + row * nWidth * 4, nCopy * 4 );
	}
}

//-----------------------------------------------------------------------------
// Device
//-----------------------------------------------------------------------------
static bool CreateBackBuffer( int nWidth, int nHeight )
{
	if ( s_BackBuffer.m_Image )
		DestroyTexture( &s_BackBuffer, false );
	s_BackBuffer.m_nWidth = nWidth > 0 ? nWidth : 1;
	s_BackBuffer.m_nHeight = nHeight > 0 ? nHeight : 1;
	s_BackBuffer.m_nMips = 1;
	s_BackBuffer.m_bRenderTarget = true;
	s_BackBuffer.m_bUsed = true;
	return EnsureResource( &s_BackBuffer );
}

static bool CreateSwapchain()
{
	VkSurfaceCapabilitiesKHR caps;
	if ( vkGetPhysicalDeviceSurfaceCapabilitiesKHR_( s_PhysicalDevice, s_Surface, &caps ) != VK_SUCCESS )
		return false;
	VkExtent2D extent = caps.currentExtent;
	if ( extent.width == 0xFFFFFFFF )
	{
		RECT rc; GetClientRect( s_hWnd, &rc );
		extent.width = rc.right - rc.left; extent.height = rc.bottom - rc.top;
	}
	if ( extent.width == 0 || extent.height == 0 )
		return false;	// minimised; try again later

	uint32_t nFormats = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR_( s_PhysicalDevice, s_Surface, &nFormats, NULL );
	VkSurfaceFormatKHR formats[64];
	if ( nFormats > 64 ) nFormats = 64;
	vkGetPhysicalDeviceSurfaceFormatsKHR_( s_PhysicalDevice, s_Surface, &nFormats, formats );
	VkSurfaceFormatKHR chosen = formats[0];
	for ( uint32_t i = 0; i < nFormats; ++i )
		if ( formats[i].format == VK_FORMAT_B8G8R8A8_UNORM )
			chosen = formats[i];

	VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	info.surface = s_Surface;
	info.minImageCount = caps.minImageCount + 1;
	if ( caps.maxImageCount && info.minImageCount > caps.maxImageCount )
		info.minImageCount = caps.maxImageCount;
	info.imageFormat = chosen.format;
	info.imageColorSpace = chosen.colorSpace;
	info.imageExtent = extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = caps.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	info.clipped = VK_TRUE;
	info.oldSwapchain = s_Swapchain;
	VkSwapchainKHR swapchain;
	if ( vkCreateSwapchainKHR_( s_Device, &info, NULL, &swapchain ) != VK_SUCCESS )
		return false;
	if ( s_Swapchain )
		vkDestroySwapchainKHR_( s_Device, s_Swapchain, NULL );
	s_Swapchain = swapchain;
	s_nSwapImages = 8;
	vkGetSwapchainImagesKHR_( s_Device, s_Swapchain, &s_nSwapImages, s_SwapImages );
	s_nSwapWidth = extent.width; s_nSwapHeight = extent.height;
	return true;
}

static bool LoadLoader()
{
	if ( vkGetInstanceProcAddr_ )
		return true;
	s_hVulkan = LoadLibraryA( "vulkan-1.dll" );
	if ( !s_hVulkan )
		return false;
	vkGetInstanceProcAddr_ = (PFN_vkGetInstanceProcAddr)GetProcAddress( s_hVulkan, "vkGetInstanceProcAddr" );
	return vkGetInstanceProcAddr_ != NULL;
}

bool Vk_Init( void *hWnd, int nWidth, int nHeight )
{
	if ( s_Device && s_hWnd == (HWND)hWnd )
		return true;
	Vk_Shutdown();
	if ( !LoadLoader() )
		return false;
	s_hWnd = (HWND)hWnd;

	vkCreateInstance_ = (PFN_vkCreateInstance)vkGetInstanceProcAddr_( NULL, "vkCreateInstance" );
	if ( !vkCreateInstance_ )
		return false;
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "GammaSource";
	app.pEngineName = "Source";
	app.apiVersion = VK_API_VERSION_1_1;
	const char *instanceExtensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = 2;
	ici.ppEnabledExtensionNames = instanceExtensions;
	if ( vkCreateInstance_( &ici, NULL, &s_Instance ) != VK_SUCCESS )
		return false;
#define LOAD_INSTANCE( name ) name##_ = (PFN_##name)vkGetInstanceProcAddr_( s_Instance, #name ); if ( !name##_ ) return false;
	VK_INSTANCE_FUNCS( LOAD_INSTANCE )

	VkWin32SurfaceCreateInfoKHR sci = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	sci.hinstance = GetModuleHandle( NULL );
	sci.hwnd = s_hWnd;
	if ( vkCreateWin32SurfaceKHR_( s_Instance, &sci, NULL, &s_Surface ) != VK_SUCCESS )
		return false;

	// first discrete GPU that can present to the window, else any that can
	uint32_t nDevices = 0;
	vkEnumeratePhysicalDevices_( s_Instance, &nDevices, NULL );
	VkPhysicalDevice devices[16];
	if ( nDevices > 16 ) nDevices = 16;
	vkEnumeratePhysicalDevices_( s_Instance, &nDevices, devices );
	int nBestScore = -1;
	for ( uint32_t d = 0; d < nDevices; ++d )
	{
		uint32_t nFamilies = 0;
		vkGetPhysicalDeviceQueueFamilyProperties_( devices[d], &nFamilies, NULL );
		VkQueueFamilyProperties families[32];
		if ( nFamilies > 32 ) nFamilies = 32;
		vkGetPhysicalDeviceQueueFamilyProperties_( devices[d], &nFamilies, families );
		for ( uint32_t f = 0; f < nFamilies; ++f )
		{
			VkBool32 bPresent = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR_( devices[d], f, s_Surface, &bPresent );
			if ( !bPresent || !( families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT ) )
				continue;
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties_( devices[d], &props );
			int nScore = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
			if ( nScore > nBestScore )
			{
				nBestScore = nScore;
				s_PhysicalDevice = devices[d];
				s_nQueueFamily = f;
				strncpy( s_szAdapterName, props.deviceName, sizeof( s_szAdapterName ) - 1 );
			}
			break;
		}
	}
	if ( !s_PhysicalDevice )
		return false;
	vkGetPhysicalDeviceMemoryProperties_( s_PhysicalDevice, &s_MemoryProps );
	static const VkFormat s_DepthFormats[] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT };
	for ( int i = 0; i < 3; ++i )
	{
		VkFormatProperties fp;
		vkGetPhysicalDeviceFormatProperties_( s_PhysicalDevice, s_DepthFormats[i], &fp );
		if ( fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT )
		{
			s_DepthFormat = s_DepthFormats[i];
			break;
		}
	}

	float flPriority = 1.0f;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qci.queueFamilyIndex = s_nQueueFamily;
	qci.queueCount = 1;
	qci.pQueuePriorities = &flPriority;
	const char *deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = 1;
	dci.ppEnabledExtensionNames = deviceExtensions;
	if ( vkCreateDevice_( s_PhysicalDevice, &dci, NULL, &s_Device ) != VK_SUCCESS )
		return false;
#define LOAD_DEVICE( name ) name##_ = (PFN_##name)vkGetDeviceProcAddr_( s_Device, #name ); if ( !name##_ ) return false;
	VK_DEVICE_FUNCS( LOAD_DEVICE )
	vkGetDeviceQueue_( s_Device, s_nQueueFamily, 0, &s_Queue );

	VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpci.queueFamilyIndex = s_nQueueFamily;
	if ( vkCreateCommandPool_( s_Device, &cpci, NULL, &s_CommandPool ) != VK_SUCCESS )
		return false;
	VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cbai.commandPool = s_CommandPool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = FRAMES;
	if ( vkAllocateCommandBuffers_( s_Device, &cbai, s_Commands ) != VK_SUCCESS )
		return false;
	for ( int i = 0; i < FRAMES; ++i )
	{
		VkSemaphoreCreateInfo semci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		vkCreateSemaphore_( s_Device, &semci, NULL, &s_ImageReady[i] );
		vkCreateSemaphore_( s_Device, &semci, NULL, &s_RenderDone[i] );
		VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCreateFence_( s_Device, &fci, NULL, &s_Fences[i] );
	}

	// one render pass for every target: keeps what is there, stays in GENERAL
	VkAttachmentDescription att[2] = {};
	att[0].format = VK_FORMAT_B8G8R8A8_UNORM;
	att[0].samples = VK_SAMPLE_COUNT_1_BIT;
	att[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	att[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att[0].initialLayout = att[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	att[1].format = s_DepthFormat;
	att[1].samples = VK_SAMPLE_COUNT_1_BIT;
	att[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	att[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	att[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	att[1].initialLayout = att[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_GENERAL };
	VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sub = {};
	sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sub.colorAttachmentCount = 1;
	sub.pColorAttachments = &colorRef;
	sub.pDepthStencilAttachment = &depthRef;
	VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpci.attachmentCount = 2; rpci.pAttachments = att;
	rpci.subpassCount = 1; rpci.pSubpasses = &sub;
	if ( vkCreateRenderPass_( s_Device, &rpci, NULL, &s_RenderPass ) != VK_SUCCESS )
		return false;

	CreateSwapchain();	// may fail while minimised; Present retries
	RECT rc; GetClientRect( s_hWnd, &rc );
	if ( nWidth <= 0 || nHeight <= 0 ) { nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top; }
	return CreateBackBuffer( nWidth, nHeight );
}

bool Vk_Resize( int nWidth, int nHeight )
{
	if ( !s_Device )
		return false;
	if ( nWidth <= 0 || nHeight <= 0 )
	{
		RECT rc; GetClientRect( s_hWnd, &rc );
		nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top;
	}
	if ( nWidth <= 0 || nHeight <= 0 || ( nWidth == s_BackBuffer.m_nWidth && nHeight == s_BackBuffer.m_nHeight ) )
		return true;
	if ( s_bRecording )
		SubmitAndWait();
	vkDeviceWaitIdle_( s_Device );
	return CreateBackBuffer( nWidth, nHeight );
}

void Vk_Shutdown()
{
	VkR_Shutdown();
	if ( s_Device )
	{
		vkDeviceWaitIdle_( s_Device );
		s_bRecording = false;
		s_pPassTarget = NULL;
		for ( int i = 0; i < FRAMES; ++i )
		{
			CollectGarbage( i );
			for ( size_t j = 0; j < s_Stream[i].size(); ++j )
				vkDestroyBuffer_( s_Device, s_Stream[i][j].m_Buffer, NULL );
			s_Stream[i].clear();
			if ( s_Fences[i] ) vkDestroyFence_( s_Device, s_Fences[i], NULL );
			if ( s_ImageReady[i] ) vkDestroySemaphore_( s_Device, s_ImageReady[i], NULL );
			if ( s_RenderDone[i] ) vkDestroySemaphore_( s_Device, s_RenderDone[i], NULL );
			s_Fences[i] = VK_NULL_HANDLE; s_ImageReady[i] = s_RenderDone[i] = VK_NULL_HANDLE;
		}
		if ( s_BackBuffer.m_Image ) DestroyTexture( &s_BackBuffer, false );
		if ( s_RenderPass ) vkDestroyRenderPass_( s_Device, s_RenderPass, NULL );
		if ( s_CommandPool ) vkDestroyCommandPool_( s_Device, s_CommandPool, NULL );
		if ( s_Swapchain ) vkDestroySwapchainKHR_( s_Device, s_Swapchain, NULL );
		for ( size_t b = 0; b < s_Blocks.size(); ++b )
			vkFreeMemory_( s_Device, s_Blocks[b].m_Memory, NULL );
		s_Blocks.clear();
		vkDestroyDevice_( s_Device, NULL );
	}
	if ( s_Surface ) vkDestroySurfaceKHR_( s_Instance, s_Surface, NULL );
	if ( s_Instance ) vkDestroyInstance_( s_Instance, NULL );
	memset( &s_BackBuffer, 0, sizeof( s_BackBuffer ) );
	s_RenderPass = VK_NULL_HANDLE; s_CommandPool = VK_NULL_HANDLE; s_Swapchain = VK_NULL_HANDLE;
	s_Device = VK_NULL_HANDLE; s_Surface = VK_NULL_HANDLE; s_Instance = VK_NULL_HANDLE;
	s_PhysicalDevice = VK_NULL_HANDLE; s_hWnd = NULL; s_nSwapImages = 0;
}

bool Vk_IsActive()
{
	return s_Device != VK_NULL_HANDLE;
}

void Vk_SetClearColor( float r, float g, float b, float a )
{
	s_ClearColor[0] = r; s_ClearColor[1] = g; s_ClearColor[2] = b; s_ClearColor[3] = a;
}

void Vk_Present()
{
	if ( !s_Device )
		return;
	Vk_Resize( 0, 0 );
	BeginRecording();
	EndPass();

	// (re)create the swap chain when the window changed
	RECT rc; GetClientRect( s_hWnd, &rc );
	int nWidth = rc.right - rc.left, nHeight = rc.bottom - rc.top;
	if ( nWidth > 0 && nHeight > 0 && ( !s_Swapchain || nWidth != s_nSwapWidth || nHeight != s_nSwapHeight ) )
	{
		SubmitAndWait();
		vkDeviceWaitIdle_( s_Device );
		CreateSwapchain();
	}

	uint32_t nImage = 0;
	bool bHaveImage = false;
	if ( s_Swapchain )
	{
		VkResult res = vkAcquireNextImageKHR_( s_Device, s_Swapchain, UINT64_MAX, s_ImageReady[s_nFrame], VK_NULL_HANDLE, &nImage );
		bHaveImage = ( res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR );
		if ( res == VK_ERROR_OUT_OF_DATE_KHR )
			s_nSwapWidth = 0;	// recreate next frame
	}
	if ( bHaveImage )
	{
		VkImage swapImage = s_SwapImages[nImage];
		ImageBarrier( swapImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
		VkImageBlit blit = {};
		blit.srcSubresource.aspectMask = blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.layerCount = blit.dstSubresource.layerCount = 1;
		blit.srcOffsets[1].x = s_BackBuffer.m_nWidth; blit.srcOffsets[1].y = s_BackBuffer.m_nHeight; blit.srcOffsets[1].z = 1;
		blit.dstOffsets[1].x = s_nSwapWidth; blit.dstOffsets[1].y = s_nSwapHeight; blit.dstOffsets[1].z = 1;
		vkCmdBlitImage_( s_Cmd, s_BackBuffer.m_Image, VK_IMAGE_LAYOUT_GENERAL, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_LINEAR );
		ImageBarrier( swapImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
	}
	vkEndCommandBuffer_( s_Cmd );
	vkResetFences_( s_Device, 1, &s_Fences[s_nFrame] );
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	if ( bHaveImage )
	{
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &s_ImageReady[s_nFrame];
		submit.pWaitDstStageMask = &waitStage;
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &s_RenderDone[s_nFrame];
	}
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &s_Cmd;
	vkQueueSubmit_( s_Queue, 1, &submit, s_Fences[s_nFrame] );
	if ( bHaveImage )
	{
		VkPresentInfoKHR present = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		present.waitSemaphoreCount = 1;
		present.pWaitSemaphores = &s_RenderDone[s_nFrame];
		present.swapchainCount = 1;
		present.pSwapchains = &s_Swapchain;
		present.pImageIndices = &nImage;
		vkQueuePresentKHR_( s_Queue, &present );
	}
	s_bRecording = false;
	s_nFrame = ( s_nFrame + 1 ) % FRAMES;
}

void Vk_GetBackBufferSize( int &nWidth, int &nHeight )
{
	nWidth = s_BackBuffer.m_nWidth; nHeight = s_BackBuffer.m_nHeight;
}

void Vk_GetWindowSize( int &nWidth, int &nHeight )
{
	RECT rc = {};
	if ( s_hWnd ) GetClientRect( s_hWnd, &rc );
	nWidth = rc.right - rc.left; nHeight = rc.bottom - rc.top;
}

void Vk_GetDesktopSize( int &nWidth, int &nHeight )
{
	nWidth = GetSystemMetrics( SM_CXSCREEN ); nHeight = GetSystemMetrics( SM_CYSCREEN );
}

const char *Vk_AdapterName()
{
	return s_szAdapterName;
}

//-----------------------------------------------------------------------------
// Renderer
//-----------------------------------------------------------------------------
bool VkR_Init()
{
	if ( s_PipelineLayout )
		return true;
	if ( !s_Device )
		return false;
	VkShaderModuleCreateInfo smci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	smci.codeSize = sizeof( g_VkVertexShader );
	smci.pCode = g_VkVertexShader;
	if ( vkCreateShaderModule_( s_Device, &smci, NULL, &s_VS ) != VK_SUCCESS )
		return false;
	smci.codeSize = sizeof( g_VkFragmentShader );
	smci.pCode = g_VkFragmentShader;
	if ( vkCreateShaderModule_( s_Device, &smci, NULL, &s_FS ) != VK_SUCCESS )
		return false;

	VkDescriptorSetLayoutBinding bindings[2] = {};
	for ( int i = 0; i < 2; ++i )
	{
		bindings[i].binding = i;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo dslci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	dslci.bindingCount = 2; dslci.pBindings = bindings;
	if ( vkCreateDescriptorSetLayout_( s_Device, &dslci, NULL, &s_SetLayout ) != VK_SUCCESS )
		return false;
	VkPushConstantRange range = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConstants_t ) };
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1; plci.pSetLayouts = &s_SetLayout;
	plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &range;
	if ( vkCreatePipelineLayout_( s_Device, &plci, NULL, &s_PipelineLayout ) != VK_SUCCESS )
		return false;

	for ( int i = 0; i < 4; ++i )
	{
		VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
		sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sci.addressModeU = ( i & 1 ) ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeV = ( i & 2 ) ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.maxLod = 1000.0f;
		vkCreateSampler_( s_Device, &sci, NULL, &s_Samplers[i] );
	}

	static const unsigned char s_White[4] = { 255, 255, 255, 255 };
	s_hWhite = VkR_CreateTexture( 1, 1, 1 );
	VkR_TexImage( s_hWhite, 0, 1, 1, s_White );

	memset( &s_State, 0, sizeof( s_State ) );
	s_State.m_bDepthTest = true;
	s_State.m_bDepthWrite = true;
	s_State.m_nDepthFunc = 3;	// near or equal
	for ( int i = 0; i < 16; ++i )
		s_WVP[i] = ( i % 5 ) ? 0.0f : 1.0f;
	return true;
}

void VkR_Shutdown()
{
	if ( !s_Device )
		return;
	vkDeviceWaitIdle_( s_Device );
	for ( size_t i = 0; i < s_Textures.size(); ++i )
		if ( s_Textures[i].m_bUsed )
			DestroyTexture( &s_Textures[i], false );
	s_Textures.clear();
	VkR_ReleaseCachedVertices( NULL );
	for ( std::map< unsigned int, VkPipeline >::iterator it = s_Pipelines.begin(); it != s_Pipelines.end(); ++it )
		if ( it->second ) vkDestroyPipeline_( s_Device, it->second, NULL );
	s_Pipelines.clear();
	for ( int f = 0; f < FRAMES; ++f )
	{
		for ( size_t i = 0; i < s_DescriptorPools[f].size(); ++i )
			vkDestroyDescriptorPool_( s_Device, s_DescriptorPools[f][i], NULL );
		s_DescriptorPools[f].clear();
	}
	for ( int i = 0; i < 4; ++i )
		if ( s_Samplers[i] ) { vkDestroySampler_( s_Device, s_Samplers[i], NULL ); s_Samplers[i] = VK_NULL_HANDLE; }
	if ( s_PipelineLayout ) vkDestroyPipelineLayout_( s_Device, s_PipelineLayout, NULL );
	if ( s_SetLayout ) vkDestroyDescriptorSetLayout_( s_Device, s_SetLayout, NULL );
	if ( s_VS ) vkDestroyShaderModule_( s_Device, s_VS, NULL );
	if ( s_FS ) vkDestroyShaderModule_( s_Device, s_FS, NULL );
	s_PipelineLayout = VK_NULL_HANDLE; s_SetLayout = VK_NULL_HANDLE; s_VS = s_FS = VK_NULL_HANDLE;
	s_hWhite = s_hBound = s_hLightmap = s_hRenderTarget = 0;
}

int VkR_CreateTexture( int nWidth, int nHeight, int nMips, bool bRenderTarget )
{
	Texture_t tex = {};
	tex.m_nWidth = nWidth > 0 ? nWidth : 1;
	tex.m_nHeight = nHeight > 0 ? nHeight : 1;
	tex.m_nMips = ( nMips > 0 && !bRenderTarget ) ? nMips : 1;
	tex.m_bRenderTarget = bRenderTarget;
	tex.m_bUsed = true;
	for ( size_t i = 0; i < s_Textures.size(); ++i )
	{
		if ( !s_Textures[i].m_bUsed )
		{
			s_Textures[i] = tex;
			return (int)i + 1;
		}
	}
	s_Textures.push_back( tex );
	return (int)s_Textures.size();
}

void VkR_DeleteTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return;
	if ( hTexture == s_hRenderTarget )
		VkR_SetRenderTarget( 0 );
	if ( s_pPassTarget == pTex )
		EndPass();
	DestroyTexture( pTex, true );
}

bool VkR_IsTexture( int hTexture )
{
	return GetTexture( hTexture ) != NULL;
}

void VkR_TexImage( int hTexture, int nLevel, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nLevel >= pTex->m_nMips )
		return;
	if ( nLevel == 0 && ( nWidth != pTex->m_nWidth || nHeight != pTex->m_nHeight ) && !pTex->m_Image )
	{
		pTex->m_nWidth = nWidth;
		pTex->m_nHeight = nHeight;
	}
	if ( !EnsureResource( pTex ) )
	{
		KeepPending( pTex, nLevel, 0, 0, nWidth, nHeight, pBGRA );
		return;
	}
	UploadRegion( pTex, nLevel, 0, 0, nWidth, nHeight, pBGRA, nWidth * 4 );
}

void VkR_TexSubImage( int hTexture, int nLevel, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nLevel >= pTex->m_nMips )
		return;
	if ( !EnsureResource( pTex ) )
	{
		KeepPending( pTex, nLevel, x, y, nWidth, nHeight, pBGRA );
		return;
	}
	UploadRegion( pTex, nLevel, x, y, nWidth, nHeight, pBGRA, nWidth * 4 );
}

unsigned char *VkR_LockShadow( int hTexture, int &nPitch, int &nWidth, int &nHeight )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return NULL;
	if ( !pTex->m_pShadow )
		pTex->m_pShadow = (unsigned char *)calloc( pTex->m_nWidth * pTex->m_nHeight, 4 );
	nPitch = pTex->m_nWidth * 4;
	nWidth = pTex->m_nWidth;
	nHeight = pTex->m_nHeight;
	return pTex->m_pShadow;
}

void VkR_UploadRect( int hTexture, int x, int y, int nWidth, int nHeight, const unsigned char *pBGRA, int nPitch )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex || !pBGRA || nWidth <= 0 || nHeight <= 0 )
		return;
	if ( !EnsureResource( pTex ) )
	{
		for ( int row = 0; row < nHeight; ++row )
			KeepPending( pTex, 0, x, y + row, nWidth, 1, pBGRA + row * nPitch );
		return;
	}
	UploadRegion( pTex, 0, x, y, nWidth, nHeight, pBGRA, nPitch );
}

void VkR_TexWrap( int hTexture, int nCoord, bool bClamp )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( !pTex )
		return;
	if ( nCoord == 0 )
		pTex->m_bClampS = bClamp;
	else if ( nCoord == 1 )
		pTex->m_bClampT = bClamp;
}

void VkR_BindTexture( int nStage, int hTexture )
{
	if ( nStage == 0 )
		s_hBound = hTexture;
}

void VkR_BindLightmap( int hTexture )
{
	s_hLightmap = hTexture;
}

void VkR_SetState( const VkRState_t &state )
{
	s_State = state;
}

static void Mul( const float *a, const float *b, float *out )
{
	float r[16];
	for ( int i = 0; i < 4; ++i )
		for ( int j = 0; j < 4; ++j )
			r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] + a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
	memcpy( out, r, sizeof( r ) );
}

void VkR_SetMatrices( const float *pModel, const float *pView, const float *pProj )
{
	float mv[16];
	Mul( pModel, pView, mv );
	Mul( mv, pProj, s_WVP );
}

void VkR_SetViewport( int x, int y, int w, int h, float flMinZ, float flMaxZ )
{
	s_Viewport.x = (float)x; s_Viewport.y = (float)y;
	s_Viewport.width = (float)w; s_Viewport.height = (float)h;
	s_Viewport.minDepth = flMinZ; s_Viewport.maxDepth = flMaxZ;
	s_bViewportSet = w > 0 && h > 0;
}

void VkR_GetTextureSize( int hTexture, int &nWidth, int &nHeight )
{
	Texture_t *pTex = GetTexture( hTexture );
	nWidth = pTex ? pTex->m_nWidth : 0;
	nHeight = pTex ? pTex->m_nHeight : 0;
}

void VkR_SetRenderTarget( int hColor )
{
	Texture_t *pTex = GetTexture( hColor );
	if ( !pTex || !pTex->m_bRenderTarget || !EnsureResource( pTex ) )
		hColor = 0;
	s_hRenderTarget = hColor;
}

void VkR_Clear( bool bColor, bool bDepth )
{
	if ( !( bColor || bDepth ) || !BeginPass() )
		return;
	VkClearAttachment clears[2];
	uint32_t n = 0;
	if ( bColor )
	{
		clears[n].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		clears[n].colorAttachment = 0;
		memcpy( clears[n].clearValue.color.float32, s_ClearColor, sizeof( s_ClearColor ) );
		++n;
	}
	if ( bDepth )
	{
		clears[n].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | ( s_DepthFormat != VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_STENCIL_BIT : 0 );
		clears[n].colorAttachment = 0;
		clears[n].clearValue.depthStencil.depth = 1.0f;
		clears[n].clearValue.depthStencil.stencil = 0;
		++n;
	}
	VkClearRect rect = {};
	rect.rect.extent.width = s_pPassTarget->m_nWidth;
	rect.rect.extent.height = s_pPassTarget->m_nHeight;
	rect.layerCount = 1;
	vkCmdClearAttachments_( s_Cmd, n, clears, 1, &rect );
}

void VkR_ClearDepth()
{
	VkR_Clear( false, true );
}

void VkR_CopyRenderTargetToTexture( int hTexture, int sx, int sy, int sw, int sh, int dx, int dy )
{
	Texture_t *pDst = GetTexture( hTexture );
	if ( !pDst || !s_Device || !EnsureResource( pDst ) || hTexture == s_hRenderTarget )
		return;
	Texture_t *pSrc = CurrentTarget();
	if ( !pSrc->m_Image )
		return;
	// clip the copy to both surfaces (no scaling here)
	if ( sx < 0 ) { dx -= sx; sw += sx; sx = 0; }
	if ( sy < 0 ) { dy -= sy; sh += sy; sy = 0; }
	if ( dx < 0 ) { sx -= dx; sw += dx; dx = 0; }
	if ( dy < 0 ) { sy -= dy; sh += dy; dy = 0; }
	if ( sx + sw > pSrc->m_nWidth ) sw = pSrc->m_nWidth - sx;
	if ( sy + sh > pSrc->m_nHeight ) sh = pSrc->m_nHeight - sy;
	if ( dx + sw > pDst->m_nWidth ) sw = pDst->m_nWidth - dx;
	if ( dy + sh > pDst->m_nHeight ) sh = pDst->m_nHeight - dy;
	if ( sw <= 0 || sh <= 0 )
		return;
	BeginRecording();
	EndPass();
	VkImageLayout dstLayout = pDst->m_Layout;
	if ( dstLayout != VK_IMAGE_LAYOUT_GENERAL )
		ImageBarrier( pDst->m_Image, VK_IMAGE_ASPECT_COLOR_BIT, dstLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
	VkImageCopy copy = {};
	copy.srcSubresource.aspectMask = copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.srcSubresource.layerCount = copy.dstSubresource.layerCount = 1;
	copy.srcOffset.x = sx; copy.srcOffset.y = sy;
	copy.dstOffset.x = dx; copy.dstOffset.y = dy;
	copy.extent.width = sw; copy.extent.height = sh; copy.extent.depth = 1;
	vkCmdCopyImage_( s_Cmd, pSrc->m_Image, VK_IMAGE_LAYOUT_GENERAL, pDst->m_Image,
		dstLayout == VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy );
	if ( dstLayout != VK_IMAGE_LAYOUT_GENERAL )
	{
		ImageBarrier( pDst->m_Image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		pDst->m_Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	pDst->m_bHasData = true;
}

//-----------------------------------------------------------------------------
// Static mesh vertex cache
//-----------------------------------------------------------------------------
struct CachedVertices_t
{
	const void *m_pKey;
	int m_nVersion;
	VkBuffer m_Buffer;
	Allocation_t m_Memory;
};
static std::vector< CachedVertices_t > s_VertexCache;

static CachedVertices_t *FindCached( const void *pCacheKey )
{
	for ( size_t i = 0; i < s_VertexCache.size(); ++i )
		if ( s_VertexCache[i].m_pKey == pCacheKey )
			return &s_VertexCache[i];
	return NULL;
}

bool VkR_HasCachedVertices( const void *pCacheKey, int nVersion )
{
	CachedVertices_t *pCached = pCacheKey ? FindCached( pCacheKey ) : NULL;
	return pCached && pCached->m_nVersion == nVersion && pCached->m_Buffer;
}

static void RetireCached( CachedVertices_t &c )
{
	if ( !c.m_Buffer )
		return;
	Garbage_t g = {};
	g.m_Buffer = c.m_Buffer;
	g.m_Memory = c.m_Memory;
	if ( s_Device ) s_Garbage[s_nFrame].push_back( g );
	c.m_Buffer = VK_NULL_HANDLE;
	memset( &c.m_Memory, 0, sizeof( c.m_Memory ) );
}

// NULL releases everything
void VkR_ReleaseCachedVertices( const void *pCacheKey )
{
	for ( size_t i = 0; i < s_VertexCache.size(); )
	{
		if ( pCacheKey && s_VertexCache[i].m_pKey != pCacheKey )
		{
			++i;
			continue;
		}
		RetireCached( s_VertexCache[i] );
		s_VertexCache.erase( s_VertexCache.begin() + i );
		if ( pCacheKey )
			return;
	}
}

static VkBuffer CacheVertices( const void *pCacheKey, int nVersion, const VkRVertex_t *pVerts, int nVerts )
{
	CachedVertices_t *pCached = FindCached( pCacheKey );
	if ( pCached && pCached->m_nVersion == nVersion && pCached->m_Buffer )
		return pCached->m_Buffer;
	if ( !pVerts )
		return VK_NULL_HANDLE;
	if ( !pCached )
	{
		CachedVertices_t entry = {};
		entry.m_pKey = pCacheKey;
		s_VertexCache.push_back( entry );
		pCached = &s_VertexCache.back();
	}
	RetireCached( *pCached );	// the GPU may still be reading the old one
	VkDeviceSize nBytes = (VkDeviceSize)nVerts * sizeof( VkRVertex_t );
	if ( !CreateBuffer( nBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		pCached->m_Buffer, pCached->m_Memory ) || !pCached->m_Memory.m_pMapped )
		return VK_NULL_HANDLE;
	memcpy( pCached->m_Memory.m_pMapped, pVerts, (size_t)nBytes );
	pCached->m_nVersion = nVersion;
	return pCached->m_Buffer;
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------
static VkDescriptorSet AllocateSet()
{
	std::vector< VkDescriptorPool > &pools = s_DescriptorPools[s_nFrame];
	size_t &nPool = s_nDescriptorPool[s_nFrame];
	for ( ;; )
	{
		if ( nPool >= pools.size() )
		{
			VkDescriptorPoolSize size = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * 4096 };
			VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			dpci.maxSets = 4096;
			dpci.poolSizeCount = 1;
			dpci.pPoolSizes = &size;
			VkDescriptorPool pool;
			if ( vkCreateDescriptorPool_( s_Device, &dpci, NULL, &pool ) != VK_SUCCESS )
				return VK_NULL_HANDLE;
			pools.push_back( pool );
		}
		VkDescriptorSetAllocateInfo dsai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		dsai.descriptorPool = pools[nPool];
		dsai.descriptorSetCount = 1;
		dsai.pSetLayouts = &s_SetLayout;
		VkDescriptorSet set;
		if ( vkAllocateDescriptorSets_( s_Device, &dsai, &set ) == VK_SUCCESS )
			return set;
		++nPool;	// full: next pool
	}
}

static Texture_t *SampleableTexture( int hTexture )
{
	Texture_t *pTex = GetTexture( hTexture );
	if ( pTex && !pTex->m_Image )
		EnsureResource( pTex );
	if ( !pTex || !pTex->m_View || !pTex->m_bHasData || pTex == s_pPassTarget )
		return NULL;
	return pTex;
}

void VkR_Draw( VkRPrimitive_t type, const VkRVertex_t *pVerts, int nVerts, const unsigned short *pIndices, int nIndices,
	const void *pCacheKey, int nVersion )
{
	if ( !s_PipelineLayout || nVerts <= 0 )
		return;
	BeginRecording();
	if ( StreamUsed() > 256 * 1024 * 1024 )
		SubmitAndWait();

	// vertex data: cached for static meshes, streamed otherwise
	VkBuffer vb = VK_NULL_HANDLE;
	VkDeviceSize nVBOffset = 0;
	if ( pCacheKey )
	{
		vb = CacheVertices( pCacheKey, nVersion, pVerts, nVerts );
	}
	else if ( pVerts )
	{
		unsigned char *p = StreamAlloc( (VkDeviceSize)nVerts * sizeof( VkRVertex_t ), vb, nVBOffset );
		if ( p ) memcpy( p, pVerts, nVerts * sizeof( VkRVertex_t ) );
		else vb = VK_NULL_HANDLE;
	}
	if ( !vb )
		return;
	VkBuffer ib = VK_NULL_HANDLE;
	VkDeviceSize nIBOffset = 0;
	if ( pIndices && nIndices > 0 )
	{
		unsigned char *p = StreamAlloc( (VkDeviceSize)nIndices * sizeof( unsigned short ), ib, nIBOffset );
		if ( !p )
			return;
		memcpy( p, pIndices, nIndices * sizeof( unsigned short ) );
	}

	// textures; a texture without data (or the current target) samples white
	Texture_t *pTex = SampleableTexture( s_hBound );
	if ( !pTex ) pTex = SampleableTexture( s_hWhite );
	Texture_t *pLightmap = SampleableTexture( s_hLightmap );
	if ( !pTex || !BeginPass() )
		return;

	VkDescriptorSet set = AllocateSet();
	if ( !set )
		return;
	VkDescriptorImageInfo images[2];
	images[0].sampler = s_Samplers[( pTex->m_bClampS ? 1 : 0 ) | ( pTex->m_bClampT ? 2 : 0 )];
	images[0].imageView = pTex->m_View;
	images[0].imageLayout = pTex->m_Layout;
	Texture_t *pSecond = pLightmap ? pLightmap : pTex;
	images[1].sampler = s_Samplers[3];
	images[1].imageView = pSecond->m_View;
	images[1].imageLayout = pSecond->m_Layout;
	VkWriteDescriptorSet writes[2] = {};
	for ( int i = 0; i < 2; ++i )
	{
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = set;
		writes[i].dstBinding = i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[i].pImageInfo = &images[i];
	}
	vkUpdateDescriptorSets_( s_Device, 2, writes, 0, NULL );

	VkPipeline pipeline = GetPipeline( type );
	if ( !pipeline )
		return;
	vkCmdBindPipeline_( s_Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
	vkCmdBindDescriptorSets_( s_Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_PipelineLayout, 0, 1, &set, 0, NULL );

	PushConstants_t pc;
	memcpy( pc.m_WVP, s_WVP, sizeof( s_WVP ) );
	pc.m_AlphaRef = s_State.m_flAlphaRef;
	pc.m_AlphaTest = s_State.m_bAlphaTest ? 1.0f : 0.0f;
	pc.m_Lightmap = pLightmap ? 1.0f : 0.0f;
	pc.m_Pad = 0.0f;
	vkCmdPushConstants_( s_Cmd, s_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pc ), &pc );

	// Direct3D-style viewport: flip y with a negative height
	VkViewport vp = s_Viewport;
	if ( !s_bViewportSet )
	{
		vp.x = vp.y = 0.0f;
		vp.width = (float)s_pPassTarget->m_nWidth; vp.height = (float)s_pPassTarget->m_nHeight;
		vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
	}
	vp.y += vp.height;
	vp.height = -vp.height;
	vkCmdSetViewport_( s_Cmd, 0, 1, &vp );
	VkRect2D scissor = {};
	scissor.extent.width = s_pPassTarget->m_nWidth;
	scissor.extent.height = s_pPassTarget->m_nHeight;
	vkCmdSetScissor_( s_Cmd, 0, 1, &scissor );

	++g_nVkDraws;
	vkCmdBindVertexBuffers_( s_Cmd, 0, 1, &vb, &nVBOffset );
	if ( ib )
	{
		vkCmdBindIndexBuffer_( s_Cmd, ib, nIBOffset, VK_INDEX_TYPE_UINT16 );
		vkCmdDrawIndexed_( s_Cmd, nIndices, 1, 0, 0, 0 );
	}
	else
	{
		vkCmdDraw_( s_Cmd, nVerts, 1, 0, 0 );
	}
}
