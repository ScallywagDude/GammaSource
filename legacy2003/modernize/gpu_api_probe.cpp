// Standalone Windows GPU API diagnostic (not an engine renderer implementation).
// Build from a VS 2026 Native Tools prompt: modernize\build_gpu_probe.cmd
#ifndef _WIN32
#include <cstdio>
int main() { std::puts("GPU probe requires Windows."); return 1; }
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <d3d11.h>
#include <d3d12.h>
#include <cstdio>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")

static void CheckD3D11()
{
    ID3D11Device *device = 0;
    ID3D11DeviceContext *context = 0;
    D3D_FEATURE_LEVEL selected = D3D_FEATURE_LEVEL_9_1;
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
    };
    HRESULT result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0,
                                       levels, 4, D3D11_SDK_VERSION,
                                       &device, &selected, &context);
    if (result == E_INVALIDARG)
        result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0,
                                    levels + 1, 3, D3D11_SDK_VERSION,
                                    &device, &selected, &context);
    if (SUCCEEDED(result))
        std::printf("D3D11 hardware device: yes (feature level 0x%04x)\n", (unsigned)selected);
    else
        std::printf("D3D11 hardware device: no (HRESULT 0x%08lx)\n", (unsigned long)result);
    if (context) context->Release();
    if (device) device->Release();
}

static void CheckD3D12()
{
    ID3D12Device *device = 0;
    HRESULT result = D3D12CreateDevice(0, D3D_FEATURE_LEVEL_11_0,
                                        IID_PPV_ARGS(&device));
    if (SUCCEEDED(result)) std::puts("D3D12 hardware device: yes");
    else std::printf("D3D12 hardware device: no (HRESULT 0x%08lx)\n", (unsigned long)result);
    if (device) device->Release();
}

static void CheckVulkanLoader()
{
    HMODULE lib = LoadLibraryA("vulkan-1.dll");
    if (!lib) { std::puts("Vulkan loader: not installed"); return; }
    typedef int (WINAPI *EnumerateVersion)(unsigned *);
    EnumerateVersion enumerate =
        reinterpret_cast<EnumerateVersion>(GetProcAddress(lib,"vkEnumerateInstanceVersion"));
    unsigned version = (1u << 22); // vkEnumerateInstanceVersion is absent from Vulkan 1.0.
    if (enumerate && enumerate(&version) != 0)
        std::puts("Vulkan loader: present, but version query failed");
    else
        std::printf("Vulkan loader: %u.%u.%u (device support NOT tested)\n",
                    version >> 22, (version >> 12) & 0x3ff, version & 0xfff);
    FreeLibrary(lib);
}

// Windows opengl32.dll only exports GL 1.1. A driver may support modern
// OpenGL through WGL context creation; query that instead of trusting 1.1.
#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif
typedef HGLRC (WINAPI *CreateContextAttribsARB)(HDC, HGLRC, const int *);

static bool IsRealWglExtension(void *address)
{
    return address && address != reinterpret_cast<void *>(1) &&
           address != reinterpret_cast<void *>(2) &&
           address != reinterpret_cast<void *>(3) &&
           address != reinterpret_cast<void *>(-1);
}

static void CheckOpenGL()
{
    HINSTANCE instance = GetModuleHandleA(0);
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = instance;
    wc.lpszClassName = "HL2ModernGPUProbe";
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    { std::puts("OpenGL: cannot register probe window"); return; }
    HWND window = CreateWindowExA(0,wc.lpszClassName,"",WS_POPUP,
                                  0,0,16,16,0,0,instance,0);
    if (!window) { std::puts("OpenGL: cannot create probe window"); return; }
    HDC dc = GetDC(window);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize=sizeof(pfd); pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA; pfd.cColorBits=32; pfd.cDepthBits=24;
    pfd.iLayerType=PFD_MAIN_PLANE;
    int pixelFormat = dc ? ChoosePixelFormat(dc, &pfd) : 0;
    HGLRC bootstrap=0;
    if (pixelFormat && SetPixelFormat(dc,pixelFormat,&pfd))
        bootstrap = wglCreateContext(dc);
    if (bootstrap && wglMakeCurrent(dc,bootstrap))
    {
        const char *legacy=reinterpret_cast<const char *>(glGetString(GL_VERSION));
        std::printf("OpenGL default context: %s\n", legacy ? legacy : "unknown");

        // The GL 3.3 shader path needs a core-capable context. Creating a
        // *default* context does not establish support for that path.
        void *proc=reinterpret_cast<void *>(wglGetProcAddress("wglCreateContextAttribsARB"));
        CreateContextAttribsARB createModern = IsRealWglExtension(proc)
            ? reinterpret_cast<CreateContextAttribsARB>(proc) : 0;
        if (createModern)
        {
            const int attributes[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };
            HGLRC modern = createModern(dc,0,attributes);
            if (modern)
            {
                wglMakeCurrent(0,0);
                if (wglMakeCurrent(dc,modern))
                {
                    const char *version=reinterpret_cast<const char *>(glGetString(GL_VERSION));
                    const char *vendor=reinterpret_cast<const char *>(glGetString(GL_VENDOR));
                    std::printf("OpenGL 3.3+ core context: yes (%s / %s)\n",
                                version ? version : "unknown", vendor ? vendor : "unknown");
                }
                else std::puts("OpenGL 3.3+ core context: cannot make current");
                wglMakeCurrent(0,0);
                wglDeleteContext(modern);
            }
            else std::puts("OpenGL 3.3+ core context: unsupported by this driver");
        }
        else std::puts("OpenGL 3.3+ core context: WGL extension unavailable");
        wglMakeCurrent(0,0);
    }
    else std::puts("OpenGL default context: unavailable");
    if (bootstrap) wglDeleteContext(bootstrap);
    if (dc) ReleaseDC(window,dc);
    DestroyWindow(window);
    UnregisterClassA(wc.lpszClassName,instance);
}

int main()
{
    std::puts("HL2 2003 modernization: Windows GPU API capability probe");
    std::puts("This only probes system APIs. It cannot attest engine renderer support.");
    CheckD3D11(); CheckD3D12(); CheckVulkanLoader(); CheckOpenGL();
    return 0;
}
#endif
