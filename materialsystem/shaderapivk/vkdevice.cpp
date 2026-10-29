//========= GammaSource ============//
//
// Native Vulkan device for shaderapivk: instance, Win32 surface, device, swap chain on the
// game window, clears and present. vulkan-1.dll is loaded at run time, so the game still
// starts on machines without Vulkan (the launcher then falls back to DirectX 9).
//
// Stage 2 of the native renderers: the device runs and clears/presents every frame.
// Geometry, textures and shaders come in the following stages.
//
//=============================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <string.h>
#include <stdio.h>
#include "vkdevice.h"

#pragma comment( lib, "user32.lib" )

// --- loader -------------------------------------------------------------------
static HMODULE s_hVulkan;
static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr_;
#define VK_FUNC( name ) static PFN_##name name##_;
VK_FUNC( vkCreateInstance ) VK_FUNC( vkDestroyInstance ) VK_FUNC( vkEnumeratePhysicalDevices )
VK_FUNC( vkGetPhysicalDeviceProperties ) VK_FUNC( vkGetPhysicalDeviceQueueFamilyProperties )
VK_FUNC( vkGetPhysicalDeviceSurfaceSupportKHR ) VK_FUNC( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
VK_FUNC( vkGetPhysicalDeviceSurfaceFormatsKHR ) VK_FUNC( vkCreateWin32SurfaceKHR ) VK_FUNC( vkDestroySurfaceKHR )
VK_FUNC( vkCreateDevice ) VK_FUNC( vkDestroyDevice ) VK_FUNC( vkGetDeviceProcAddr )
VK_FUNC( vkGetDeviceQueue ) VK_FUNC( vkDeviceWaitIdle ) VK_FUNC( vkCreateSwapchainKHR ) VK_FUNC( vkDestroySwapchainKHR )
VK_FUNC( vkGetSwapchainImagesKHR ) VK_FUNC( vkAcquireNextImageKHR ) VK_FUNC( vkQueuePresentKHR )
VK_FUNC( vkCreateCommandPool ) VK_FUNC( vkDestroyCommandPool ) VK_FUNC( vkAllocateCommandBuffers )
VK_FUNC( vkBeginCommandBuffer ) VK_FUNC( vkEndCommandBuffer ) VK_FUNC( vkResetCommandBuffer )
VK_FUNC( vkCmdPipelineBarrier ) VK_FUNC( vkCmdClearColorImage ) VK_FUNC( vkQueueSubmit )
VK_FUNC( vkCreateSemaphore ) VK_FUNC( vkDestroySemaphore ) VK_FUNC( vkCreateFence ) VK_FUNC( vkDestroyFence )
VK_FUNC( vkWaitForFences ) VK_FUNC( vkResetFences )

static VkInstance s_Instance;
static VkPhysicalDevice s_PhysicalDevice;
static VkSurfaceKHR s_Surface;
static VkDevice s_Device;
static VkQueue s_Queue;
static uint32_t s_nQueueFamily;
static VkSwapchainKHR s_Swapchain;
static VkFormat s_Format = VK_FORMAT_B8G8R8A8_UNORM;
static VkImage s_Images[8];
static uint32_t s_nImages;
static VkCommandPool s_CommandPool;
static VkCommandBuffer s_Commands;
static VkSemaphore s_ImageReady, s_RenderDone;
static VkFence s_FrameFence;
static HWND s_hWnd;
static int s_nWidth, s_nHeight;
static VkClearColorValue s_ClearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };
static bool s_bClearPending = true;
static char s_szAdapterName[256] = "Vulkan";

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

#define LOAD_GLOBAL( name ) name##_ = (PFN_##name)vkGetInstanceProcAddr_( NULL, #name );
#define LOAD_INSTANCE( name ) name##_ = (PFN_##name)vkGetInstanceProcAddr_( s_Instance, #name ); if ( !name##_ ) return false;
#define LOAD_DEVICE( name ) name##_ = (PFN_##name)vkGetDeviceProcAddr_( s_Device, #name ); if ( !name##_ ) return false;

static bool CreateSwapchain()
{
	VkSurfaceCapabilitiesKHR caps;
	if ( vkGetPhysicalDeviceSurfaceCapabilitiesKHR_( s_PhysicalDevice, s_Surface, &caps ) != VK_SUCCESS )
		return false;
	VkExtent2D extent = caps.currentExtent;
	if ( extent.width == 0xFFFFFFFF )
	{
		extent.width = s_nWidth; extent.height = s_nHeight;
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
	s_Format = chosen.format;

	VkSwapchainCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
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
	s_nImages = 8;
	vkGetSwapchainImagesKHR_( s_Device, s_Swapchain, &s_nImages, s_Images );
	s_nWidth = extent.width; s_nHeight = extent.height;
	return true;
}

bool Vk_Init( void *hWnd, int nWidth, int nHeight )
{
	if ( s_Device && s_hWnd == (HWND)hWnd )
		return true;
	Vk_Shutdown();
	if ( !LoadLoader() )
		return false;
	s_hWnd = (HWND)hWnd;
	RECT rc; GetClientRect( s_hWnd, &rc );
	s_nWidth = nWidth > 0 ? nWidth : rc.right - rc.left;
	s_nHeight = nHeight > 0 ? nHeight : rc.bottom - rc.top;

	LOAD_GLOBAL( vkCreateInstance );
	if ( !vkCreateInstance_ )
		return false;
	VkApplicationInfo app = {};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "GammaSource";
	app.pEngineName = "Source";
	app.apiVersion = VK_API_VERSION_1_1;
	const char *instanceExtensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	VkInstanceCreateInfo ici = {};
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = 2;
	ici.ppEnabledExtensionNames = instanceExtensions;
	if ( vkCreateInstance_( &ici, NULL, &s_Instance ) != VK_SUCCESS )
		return false;

	LOAD_INSTANCE( vkDestroyInstance ) LOAD_INSTANCE( vkEnumeratePhysicalDevices ) LOAD_INSTANCE( vkGetPhysicalDeviceProperties )
	LOAD_INSTANCE( vkGetPhysicalDeviceQueueFamilyProperties ) LOAD_INSTANCE( vkGetPhysicalDeviceSurfaceSupportKHR )
	LOAD_INSTANCE( vkGetPhysicalDeviceSurfaceCapabilitiesKHR ) LOAD_INSTANCE( vkGetPhysicalDeviceSurfaceFormatsKHR )
	LOAD_INSTANCE( vkCreateWin32SurfaceKHR ) LOAD_INSTANCE( vkDestroySurfaceKHR ) LOAD_INSTANCE( vkCreateDevice )
	LOAD_INSTANCE( vkGetDeviceProcAddr )

	VkWin32SurfaceCreateInfoKHR sci = {};
	sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
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

	float flPriority = 1.0f;
	VkDeviceQueueCreateInfo qci = {};
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.queueFamilyIndex = s_nQueueFamily;
	qci.queueCount = 1;
	qci.pQueuePriorities = &flPriority;
	const char *deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo dci = {};
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = 1;
	dci.ppEnabledExtensionNames = deviceExtensions;
	if ( vkCreateDevice_( s_PhysicalDevice, &dci, NULL, &s_Device ) != VK_SUCCESS )
		return false;

	LOAD_DEVICE( vkDestroyDevice ) LOAD_DEVICE( vkGetDeviceQueue ) LOAD_DEVICE( vkDeviceWaitIdle )
	LOAD_DEVICE( vkCreateSwapchainKHR ) LOAD_DEVICE( vkDestroySwapchainKHR ) LOAD_DEVICE( vkGetSwapchainImagesKHR )
	LOAD_DEVICE( vkAcquireNextImageKHR ) LOAD_DEVICE( vkQueuePresentKHR ) LOAD_DEVICE( vkCreateCommandPool )
	LOAD_DEVICE( vkDestroyCommandPool ) LOAD_DEVICE( vkAllocateCommandBuffers ) LOAD_DEVICE( vkBeginCommandBuffer )
	LOAD_DEVICE( vkEndCommandBuffer ) LOAD_DEVICE( vkResetCommandBuffer ) LOAD_DEVICE( vkCmdPipelineBarrier )
	LOAD_DEVICE( vkCmdClearColorImage ) LOAD_DEVICE( vkQueueSubmit ) LOAD_DEVICE( vkCreateSemaphore )
	LOAD_DEVICE( vkDestroySemaphore ) LOAD_DEVICE( vkCreateFence ) LOAD_DEVICE( vkDestroyFence )
	LOAD_DEVICE( vkWaitForFences ) LOAD_DEVICE( vkResetFences )

	vkGetDeviceQueue_( s_Device, s_nQueueFamily, 0, &s_Queue );

	VkCommandPoolCreateInfo cpci = {};
	cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpci.queueFamilyIndex = s_nQueueFamily;
	if ( vkCreateCommandPool_( s_Device, &cpci, NULL, &s_CommandPool ) != VK_SUCCESS )
		return false;
	VkCommandBufferAllocateInfo cbai = {};
	cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbai.commandPool = s_CommandPool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = 1;
	if ( vkAllocateCommandBuffers_( s_Device, &cbai, &s_Commands ) != VK_SUCCESS )
		return false;
	VkSemaphoreCreateInfo semci = {};
	semci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	vkCreateSemaphore_( s_Device, &semci, NULL, &s_ImageReady );
	vkCreateSemaphore_( s_Device, &semci, NULL, &s_RenderDone );
	VkFenceCreateInfo fci = {};
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	vkCreateFence_( s_Device, &fci, NULL, &s_FrameFence );

	CreateSwapchain();	// may fail while minimised; Present retries
	return true;
}

void Vk_Shutdown()
{
	if ( s_Device )
	{
		vkDeviceWaitIdle_( s_Device );
		if ( s_FrameFence ) vkDestroyFence_( s_Device, s_FrameFence, NULL );
		if ( s_ImageReady ) vkDestroySemaphore_( s_Device, s_ImageReady, NULL );
		if ( s_RenderDone ) vkDestroySemaphore_( s_Device, s_RenderDone, NULL );
		if ( s_CommandPool ) vkDestroyCommandPool_( s_Device, s_CommandPool, NULL );
		if ( s_Swapchain ) vkDestroySwapchainKHR_( s_Device, s_Swapchain, NULL );
		vkDestroyDevice_( s_Device, NULL );
	}
	if ( s_Surface ) vkDestroySurfaceKHR_( s_Instance, s_Surface, NULL );
	if ( s_Instance ) vkDestroyInstance_( s_Instance, NULL );
	s_FrameFence = VK_NULL_HANDLE; s_ImageReady = s_RenderDone = VK_NULL_HANDLE; s_CommandPool = VK_NULL_HANDLE;
	s_Swapchain = VK_NULL_HANDLE; s_Device = VK_NULL_HANDLE; s_Surface = VK_NULL_HANDLE; s_Instance = VK_NULL_HANDLE;
	s_PhysicalDevice = VK_NULL_HANDLE; s_hWnd = NULL; s_nImages = 0;
}

bool Vk_IsActive()
{
	return s_Device != VK_NULL_HANDLE;
}

void Vk_SetClearColor( float r, float g, float b, float a )
{
	s_ClearColor.float32[0] = r; s_ClearColor.float32[1] = g; s_ClearColor.float32[2] = b; s_ClearColor.float32[3] = a;
}

void Vk_Clear( bool bColor )
{
	if ( bColor )
		s_bClearPending = true;
}

void Vk_Present()
{
	if ( !s_Device )
		return;

	RECT rc; GetClientRect( s_hWnd, &rc );
	int nWidth = rc.right - rc.left, nHeight = rc.bottom - rc.top;
	if ( nWidth <= 0 || nHeight <= 0 )
		return;
	if ( !s_Swapchain || nWidth != s_nWidth || nHeight != s_nHeight )
	{
		vkDeviceWaitIdle_( s_Device );
		s_nWidth = nWidth; s_nHeight = nHeight;
		if ( !CreateSwapchain() )
			return;
	}

	vkWaitForFences_( s_Device, 1, &s_FrameFence, VK_TRUE, UINT64_MAX );
	uint32_t nImage = 0;
	VkResult res = vkAcquireNextImageKHR_( s_Device, s_Swapchain, UINT64_MAX, s_ImageReady, VK_NULL_HANDLE, &nImage );
	if ( res == VK_ERROR_OUT_OF_DATE_KHR )
	{
		vkDeviceWaitIdle_( s_Device );
		CreateSwapchain();
		return;
	}
	if ( res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR )
		return;
	vkResetFences_( s_Device, 1, &s_FrameFence );

	vkResetCommandBuffer_( s_Commands, 0 );
	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer_( s_Commands, &begin );

	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VkImageMemoryBarrier toClear = {};
	toClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toClear.srcAccessMask = 0;
	toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toClear.srcQueueFamilyIndex = toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toClear.image = s_Images[nImage];
	toClear.subresourceRange = range;
	vkCmdPipelineBarrier_( s_Commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toClear );
	vkCmdClearColorImage_( s_Commands, s_Images[nImage], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &s_ClearColor, 1, &range );
	VkImageMemoryBarrier toPresent = toClear;
	toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toPresent.dstAccessMask = 0;
	toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	vkCmdPipelineBarrier_( s_Commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &toPresent );
	vkEndCommandBuffer_( s_Commands );
	s_bClearPending = false;

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &s_ImageReady;
	submit.pWaitDstStageMask = &waitStage;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &s_Commands;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &s_RenderDone;
	vkQueueSubmit_( s_Queue, 1, &submit, s_FrameFence );

	VkPresentInfoKHR present = {};
	present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = &s_RenderDone;
	present.swapchainCount = 1;
	present.pSwapchains = &s_Swapchain;
	present.pImageIndices = &nImage;
	vkQueuePresentKHR_( s_Queue, &present );
}

void Vk_GetBackBufferSize( int &nWidth, int &nHeight )
{
	nWidth = s_nWidth; nHeight = s_nHeight;
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
