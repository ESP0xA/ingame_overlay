#include <cassert>

#include "Renderer_Detector.h"
#include "Log.h"

#include <library/library.h>
#include <mini_detour.h>

#if defined(WIN64) || defined(_WIN64) || defined(__MINGW64__) || defined(WIN32) || defined(_WIN32) || defined(__MINGW32__)
    #define __WINDOWS__
#endif

#if defined(__linux__) || defined(linux)
    #define __LINUX__
#endif

#ifdef __WINDOWS__
#include "windows/DirectX_VTables.h"
#include "windows/DX12_Hook.h"
#include "windows/DX11_Hook.h"
#include "windows/DX10_Hook.h"
#include "windows/DX9_Hook.h"
#include "windows/OpenGL_Hook.h"
#include "windows/Vulkan_Hook.h"

static constexpr auto windowClassName = "___overlay_window_class___";
class Renderer_Detector
{
    static Renderer_Detector* instance;
public:
    static Renderer_Detector* Inst()
    {
        if (instance == nullptr)
        {
            instance = new Renderer_Detector;
        }
        return instance;
    }

    ~Renderer_Detector()
    {
        delete dx9_hook;
        delete dx10_hook;
        delete dx11_hook;
        delete dx12_hook;
        delete opengl_hook;
        delete vulkan_hook;
    }

private:
    Renderer_Detector():
        dx9_hook(nullptr),
        dx10_hook(nullptr),
        dx11_hook(nullptr),
        dx12_hook(nullptr),
        opengl_hook(nullptr),
        vulkan_hook(nullptr)
    {}

    std::mutex renderer_mutex;

    Base_Hook hooks;

    decltype(&IDXGISwapChain::Present)       IDXGISwapChainPresent;
    decltype(&IDirect3DDevice9::Present)     IDirect3DDevice9Present;
    decltype(&IDirect3DDevice9Ex::PresentEx) IDirect3DDevice9ExPresentEx;
    decltype(::SwapBuffers)*                 wglSwapBuffers;
    decltype(::vkCmdExecuteCommands)*        vkCmdExecuteCommands;
    decltype(::vkCmdDraw)*                   vkCmdDraw;
    decltype(::vkCmdDrawIndexed)*            vkCmdDrawIndexed;

    bool dxgi_hooked;
    bool dx12_hooked;
    bool dx11_hooked;
    bool dx10_hooked;
    bool dx9_hooked;
    bool opengl_hooked;
    bool vulkan_hooked;

    DX12_Hook*   dx12_hook;
    DX11_Hook*   dx11_hook;
    DX10_Hook*   dx10_hook;
    DX9_Hook*    dx9_hook;
    OpenGL_Hook* opengl_hook;
    Vulkan_Hook* vulkan_hook;
    Renderer_Hook* renderer_hook;
    
    HWND dummyWindow = nullptr;
    ATOM atom = 0;

    HWND create_hwnd()
    {
        if (dummyWindow == nullptr)
        {
            HINSTANCE hInst = GetModuleHandle(nullptr);
            if (atom == 0)
            {
                // Register a window class for creating our render window with.
                WNDCLASSEX windowClass = {};

                windowClass.cbSize = sizeof(WNDCLASSEX);
                windowClass.style = CS_HREDRAW | CS_VREDRAW;
                windowClass.lpfnWndProc = DefWindowProc;
                windowClass.cbClsExtra = 0;
                windowClass.cbWndExtra = 0;
                windowClass.hInstance = hInst;
                windowClass.hIcon = NULL;
                windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
                windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                windowClass.lpszMenuName = NULL;
                windowClass.lpszClassName = windowClassName;
                windowClass.hIconSm = NULL;

                atom = ::RegisterClassEx(&windowClass);
            }

            if (atom > 0)
            {
                dummyWindow = ::CreateWindowEx(
                    NULL,
                    windowClassName,
                    "",
                    WS_OVERLAPPEDWINDOW,
                    0,
                    0,
                    1,
                    1,
                    NULL,
                    NULL,
                    hInst,
                    nullptr
                );

                assert(dummyWindow && "Failed to create window");
            }
        }

        return dummyWindow;
    }

    void destroy_hwnd()
    {
        if (dummyWindow != nullptr)
        {
            DestroyWindow(dummyWindow);
            UnregisterClass(windowClassName, GetModuleHandle(nullptr));

            dummyWindow = nullptr;
            atom = 0;
        }
    }

    static HRESULT STDMETHODCALLTYPE MyIDXGISwapChain_Present(IDXGISwapChain* _this, UINT SyncInterval, UINT Flags)
    {
        auto res = (_this->*Inst()->IDXGISwapChainPresent)(SyncInterval, Flags);

        IUnknown* pDevice = nullptr;
        if (Inst()->dx12_hooked)
        {
            _this->GetDevice(IID_PPV_ARGS(reinterpret_cast<ID3D12Device**>(&pDevice)));
        }
        if (pDevice != nullptr)
        {
            Inst()->hooks.UnhookAll();
            Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->dx12_hook);
            Inst()->dx12_hook = nullptr;
        }
        else
        {
            if (Inst()->dx11_hooked)
            {
                _this->GetDevice(IID_PPV_ARGS(reinterpret_cast<ID3D11Device**>(&pDevice)));
            }
            if (pDevice != nullptr)
            {
                Inst()->hooks.UnhookAll();
                Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->dx11_hook);
                Inst()->dx11_hook = nullptr;
            }
            else
            {
                if (Inst()->dx10_hooked)
                {
                    _this->GetDevice(IID_PPV_ARGS(reinterpret_cast<ID3D10Device**>(&pDevice)));
                }
                if (pDevice != nullptr)
                {
                    Inst()->hooks.UnhookAll();
                    Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->dx10_hook);
                    Inst()->dx10_hook = nullptr;
                }
            }
        }
        if (pDevice != nullptr)
        {
            pDevice->Release();
        }

        return res;
    }

    static HRESULT STDMETHODCALLTYPE MyDX9Present(IDirect3DDevice9* _this, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
    {
        auto res = (_this->*Inst()->IDirect3DDevice9Present)(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
        Inst()->hooks.UnhookAll();
        Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->dx9_hook);
        Inst()->dx9_hook = nullptr;

        return res;
    }

    static HRESULT STDMETHODCALLTYPE MyDX9PresentEx(IDirect3DDevice9Ex* _this, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
    {
        auto res = (_this->*Inst()->IDirect3DDevice9ExPresentEx)(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
        Inst()->hooks.UnhookAll();
        Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->dx9_hook);
        Inst()->dx9_hook = nullptr;

        return res;
    }

    static BOOL WINAPI MywglSwapBuffers(HDC hDC)
    {
        auto res = Inst()->wglSwapBuffers(hDC);
        Inst()->hooks.UnhookAll();
        Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->opengl_hook);
        Inst()->opengl_hook = nullptr;

        return res;
    }

    static void VKAPI_CALL MyvkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
    {
        Inst()->vkCmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
        Inst()->hooks.UnhookAll();
        Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->vulkan_hook);
        Inst()->vulkan_hook = nullptr;
    }

    static void VKAPI_CALL MyvkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        Inst()->vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
        Inst()->hooks.UnhookAll();
        Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->vulkan_hook);
        Inst()->vulkan_hook = nullptr;
    }

    static void VKAPI_CALL MyvkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        Inst()->vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        Inst()->hooks.UnhookAll();
        Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->vulkan_hook);
        Inst()->vulkan_hook = nullptr;
    }

    void HookDXGIPresent(IDXGISwapChain* pSwapChain, decltype(&IDXGISwapChain::Present)& pfnPresent, decltype(&IDXGISwapChain::ResizeBuffers)& pfnResizeBuffers, decltype(&IDXGISwapChain::ResizeTarget)& pfnResizeTarget)
    {
        void** vTable = *reinterpret_cast<void***>(pSwapChain);
        (void*&)pfnPresent = vTable[(int)IDXGISwapChainVTable::Present];
        (void*&)pfnResizeBuffers = vTable[(int)IDXGISwapChainVTable::ResizeBuffers];
        (void*&)pfnResizeTarget = vTable[(int)IDXGISwapChainVTable::ResizeTarget];

        if (!dxgi_hooked)
        {
            dxgi_hooked = true;

            (void*&)IDXGISwapChainPresent = vTable[(int)IDXGISwapChainVTable::Present];
            hooks.BeginHook();
            hooks.HookFunc(std::pair<void**, void*>{ (void**)&IDXGISwapChainPresent, (void*)&MyIDXGISwapChain_Present});
            hooks.EndHook();
        }
    }

    void HookDX9Present(IDirect3DDevice9* pDevice, bool ex)
    {
        void** vTable = *reinterpret_cast<void***>(pDevice);
        (void*&)IDirect3DDevice9Present = vTable[(int)IDirect3DDevice9VTable::Present];

        hooks.BeginHook();
        hooks.HookFunc(std::pair<void**, void*>{ (void**)&IDirect3DDevice9Present, (void*)&MyDX9Present });
        hooks.EndHook();

        if (ex)
        {
            (void*&)IDirect3DDevice9ExPresentEx = vTable[(int)IDirect3DDevice9VTable::PresentEx];

            hooks.BeginHook();
            hooks.HookFunc(std::pair<void**, void*>{ (void**)&IDirect3DDevice9ExPresentEx, (void*)&MyDX9PresentEx });
            hooks.EndHook();
        }
        else
        {
            IDirect3DDevice9Present = nullptr;
        }
    }

    void HookwglSwapBuffers(decltype(::SwapBuffers)* _wglSwapBuffers)
    {
        wglSwapBuffers = _wglSwapBuffers;

        hooks.BeginHook();
        hooks.HookFunc(std::pair<void**, void*>{ (void**)&wglSwapBuffers, (void*)&MywglSwapBuffers });
        hooks.EndHook();
    }

    void HookvkCmdExecuteCommands(decltype(::vkCmdExecuteCommands)* _vkCmdExecuteCommands, decltype(::vkCmdDraw)* _vkCmdDraw, decltype(::vkCmdDrawIndexed)* _vkCmdDrawIndexed)
    {
        vkCmdExecuteCommands = _vkCmdExecuteCommands;
        vkCmdDraw = _vkCmdDraw;
        vkCmdDrawIndexed = _vkCmdDrawIndexed;

        hooks.BeginHook();
        hooks.HookFuncs(
            std::pair<void**, void*>{ (void**)&vkCmdExecuteCommands, (void*)&MyvkCmdExecuteCommands },
            std::pair<void**, void*>{ (void**)&vkCmdDraw, (void*)&MyvkCmdDraw },
            std::pair<void**, void*>{ (void**)&vkCmdDrawIndexed, (void*)&MyvkCmdDrawIndexed }
        );
        hooks.EndHook();
    }

    void hook_dx9()
    {
        if (!dx9_hooked)
        {
            IDirect3D9Ex* pD3D = nullptr;
            IUnknown* pDevice = nullptr;
            HMODULE library = GetModuleHandle(DX9_Hook::DLL_NAME);
            decltype(Direct3DCreate9Ex)* Direct3DCreate9Ex = nullptr;
            if (library != nullptr)
            {
                Direct3DCreate9Ex = (decltype(Direct3DCreate9Ex))GetProcAddress(library, "Direct3DCreate9Ex");
                D3DPRESENT_PARAMETERS params = {};
                params.BackBufferWidth = 1;
                params.BackBufferHeight = 1;
                params.hDeviceWindow = dummyWindow;
                params.BackBufferCount = 1;
                params.Windowed = TRUE;
                params.SwapEffect = D3DSWAPEFFECT_DISCARD;

                if (Direct3DCreate9Ex != nullptr)
                {
                    Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D);
                    pD3D->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, NULL, reinterpret_cast<IDirect3DDevice9Ex**>(&pDevice));
                }
                else
                {
                    decltype(Direct3DCreate9)* Direct3DCreate9 = (decltype(Direct3DCreate9))GetProcAddress(library, "Direct3DCreate9");
                    if (Direct3DCreate9 != nullptr)
                    {
                        pD3D = reinterpret_cast<IDirect3D9Ex*>(Direct3DCreate9(D3D_SDK_VERSION));
                        pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, reinterpret_cast<IDirect3DDevice9**>(&pDevice));
                    }
                }
            }

            if (pDevice != nullptr)
            {
                APP_LOGI("Hooked D3D9::Present to detect DX Version\n");

                dx9_hooked = true;
                HookDX9Present(reinterpret_cast<IDirect3DDevice9*>(pDevice), Direct3DCreate9Ex != nullptr);

                void** vTable = *reinterpret_cast<void***>(pDevice);
                decltype(&IDirect3DDevice9::Present) pfnPresent;
                decltype(&IDirect3DDevice9::Reset) pfnReset;
                decltype(&IDirect3DDevice9::EndScene) pfnEndScene;
                decltype(&IDirect3DDevice9Ex::PresentEx) pfnPresentEx = nullptr;

                (void*&)pfnPresent   = (void*)vTable[(int)IDirect3DDevice9VTable::Present];
                (void*&)pfnReset     = (void*)vTable[(int)IDirect3DDevice9VTable::Reset];
                (void*&)pfnEndScene  = (void*)vTable[(int)IDirect3DDevice9VTable::EndScene];
                if (Direct3DCreate9Ex != nullptr)
                {
                    (void*&)pfnPresentEx = (void*)vTable[(int)IDirect3DDevice9VTable::PresentEx];
                }

                dx9_hook = DX9_Hook::Inst();
                dx9_hook->loadFunctions(pfnPresent, pfnReset, pfnEndScene, pfnPresentEx);
            }
            else
            {
                APP_LOGW("Failed to hook D3D9::Present to detect DX Version\n");
            }

            if (pDevice) pDevice->Release();
            if (pD3D) pD3D->Release();
        }
    }

    void hook_dx10()
    {
        if (!dx10_hooked)
        {
            IDXGISwapChain* pSwapChain = nullptr;
            ID3D10Device* pDevice = nullptr;
            HMODULE library = GetModuleHandle(DX10_Hook::DLL_NAME);
            if (library != nullptr)
            {
                decltype(D3D10CreateDeviceAndSwapChain)* D3D10CreateDeviceAndSwapChain =
                    (decltype(D3D10CreateDeviceAndSwapChain))GetProcAddress(library, "D3D10CreateDeviceAndSwapChain");
                if (D3D10CreateDeviceAndSwapChain != nullptr)
                {
                    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};

                    SwapChainDesc.BufferCount = 1;
                    SwapChainDesc.BufferDesc.Width = 1;
                    SwapChainDesc.BufferDesc.Height = 1;
                    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    SwapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
                    SwapChainDesc.BufferDesc.RefreshRate.Denominator = 0;
                    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    SwapChainDesc.OutputWindow = dummyWindow;
                    SwapChainDesc.SampleDesc.Count = 1;
                    SwapChainDesc.SampleDesc.Quality = 0;
                    SwapChainDesc.Windowed = TRUE;

                    D3D10CreateDeviceAndSwapChain(NULL, D3D10_DRIVER_TYPE_NULL, NULL, 0, D3D10_SDK_VERSION, &SwapChainDesc, &pSwapChain, &pDevice);
                }
            }
            if (pSwapChain != nullptr)
            {
                APP_LOGI("Hooked IDXGISwapChain::Present to detect DX Version\n");

                dx10_hooked = true;

                decltype(&IDXGISwapChain::Present) pfnPresent;
                decltype(&IDXGISwapChain::ResizeBuffers) pfnResizeBuffers;
                decltype(&IDXGISwapChain::ResizeTarget) pfnResizeTarget;

                HookDXGIPresent(pSwapChain, pfnPresent, pfnResizeBuffers, pfnResizeTarget);

                dx10_hook = DX10_Hook::Inst();
                dx10_hook->loadFunctions(pfnPresent, pfnResizeBuffers, pfnResizeTarget);
            }
            else
            {
                APP_LOGW("Failed to Hook IDXGISwapChain::Present to detect DX Version\n");
            }
            if (pDevice)pDevice->Release();
            if (pSwapChain)pSwapChain->Release();
        }
    }

    void hook_dx11()
    {
        if (!dx11_hooked)
        {
            IDXGISwapChain* pSwapChain = nullptr;
            ID3D11Device* pDevice = nullptr;
            HMODULE library = GetModuleHandle(DX11_Hook::DLL_NAME);
            if (library != nullptr)
            {
                decltype(D3D11CreateDeviceAndSwapChain)* D3D11CreateDeviceAndSwapChain =
                    (decltype(D3D11CreateDeviceAndSwapChain))GetProcAddress(library, "D3D11CreateDeviceAndSwapChain");
                if (D3D11CreateDeviceAndSwapChain != nullptr)
                {
                    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};

                    SwapChainDesc.BufferCount = 1;
                    SwapChainDesc.BufferDesc.Width = 1;
                    SwapChainDesc.BufferDesc.Height = 1;
                    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    SwapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
                    SwapChainDesc.BufferDesc.RefreshRate.Denominator = 0;
                    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    SwapChainDesc.OutputWindow = dummyWindow;
                    SwapChainDesc.SampleDesc.Count = 1;
                    SwapChainDesc.SampleDesc.Quality = 0;
                    SwapChainDesc.Windowed = TRUE;

                    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_NULL, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &SwapChainDesc, &pSwapChain, &pDevice, NULL, NULL);
                }
            }
            if (pSwapChain != nullptr)
            {
                APP_LOGI("Hooked IDXGISwapChain::Present to detect DX Version\n");

                dx11_hooked = true;

                decltype(&IDXGISwapChain::Present) pfnPresent;
                decltype(&IDXGISwapChain::ResizeBuffers) pfnResizeBuffers;
                decltype(&IDXGISwapChain::ResizeTarget) pfnResizeTarget;

                HookDXGIPresent(pSwapChain, pfnPresent, pfnResizeBuffers, pfnResizeTarget);

                dx11_hook = DX11_Hook::Inst();
                dx11_hook->loadFunctions(pfnPresent, pfnResizeBuffers, pfnResizeTarget);
            }
            else
            {
                APP_LOGW("Failed to Hook IDXGISwapChain::Present to detect DX Version\n");
            }

            if (pDevice) pDevice->Release();
            if (pSwapChain) pSwapChain->Release();
        }
    }

    void hook_dx12()
    {
        if (!dx12_hooked)
        {
            IDXGIFactory4* pDXGIFactory = nullptr;
            IDXGISwapChain1* pSwapChain = nullptr;
            ID3D12CommandQueue* pCommandQueue = nullptr;
            ID3D12Device* pDevice = nullptr;
            ID3D12CommandAllocator* pCommandAllocator = nullptr;
            ID3D12GraphicsCommandList* pCommandList = nullptr;

            HMODULE library = GetModuleHandle(DX12_Hook::DLL_NAME);
            if (library != nullptr)
            {
                decltype(D3D12CreateDevice)* D3D12CreateDevice =
                    (decltype(D3D12CreateDevice))GetProcAddress(library, "D3D12CreateDevice");

                if (D3D12CreateDevice != nullptr)
                {
                    D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice));

                    if (pDevice != nullptr)
                    {
                        DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
                        SwapChainDesc.BufferCount = 2;
                        SwapChainDesc.Width = 1;
                        SwapChainDesc.Height = 1;
                        SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                        SwapChainDesc.Stereo = FALSE;
                        SwapChainDesc.SampleDesc = { 1, 0 };
                        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                        SwapChainDesc.Scaling = DXGI_SCALING_NONE;
                        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                        SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

                        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
                        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
                        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                        pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue));

                        if (pCommandQueue != nullptr)
                        {
                            HMODULE dxgi = GetModuleHandle("dxgi.dll");
                            if (dxgi != nullptr)
                            {
                                decltype(CreateDXGIFactory1)* CreateDXGIFactory1 = (decltype(CreateDXGIFactory1))GetProcAddress(dxgi, "CreateDXGIFactory1");
                                if (CreateDXGIFactory1 != nullptr)
                                {
                                    CreateDXGIFactory1(IID_PPV_ARGS(&pDXGIFactory));
                                    if (pDXGIFactory != nullptr)
                                    {
                                        pDXGIFactory->CreateSwapChainForHwnd(pCommandQueue, dummyWindow, &SwapChainDesc, NULL, NULL, &pSwapChain);

                                        pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCommandAllocator));
                                        if (pCommandAllocator != nullptr)
                                        {
                                            pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator, NULL, IID_PPV_ARGS(&pCommandList));
                                        }
                                    }
                                }
                            }
                        }
                    }//if (pDevice != nullptr)
                }//if (D3D12CreateDevice != nullptr)
            }//if (library != nullptr)
            if (pCommandQueue != nullptr && pSwapChain != nullptr)
            {
                APP_LOGI("Hooked IDXGISwapChain::Present to detect DX Version\n");

                dx12_hooked = true;

                decltype(&IDXGISwapChain::Present) pfnPresent;
                decltype(&IDXGISwapChain::ResizeBuffers) pfnResizeBuffers;
                decltype(&IDXGISwapChain::ResizeTarget) pfnResizeTarget;

                HookDXGIPresent(pSwapChain, pfnPresent, pfnResizeBuffers, pfnResizeTarget);

                void** vTable = *reinterpret_cast<void***>(pCommandList);
                decltype(&ID3D12CommandQueue::ExecuteCommandLists) pfnExecuteCommandLists;
                (void*&)pfnExecuteCommandLists = vTable[(int)ID3D12CommandQueueVTable::ExecuteCommandLists];

                dx12_hook = DX12_Hook::Inst();
                dx12_hook->loadFunctions(pfnPresent, pfnResizeBuffers, pfnResizeTarget, pfnExecuteCommandLists);
            }
            else
            {
                APP_LOGW("Failed to Hook IDXGISwapChain::Present to detect DX Version\n");
            }

            if (pCommandList) pCommandList->Release();
            if (pCommandAllocator) pCommandAllocator->Release();
            if (pSwapChain) pSwapChain->Release();
            if (pDXGIFactory) pDXGIFactory->Release();
            if (pCommandQueue) pCommandQueue->Release();
            if (pDevice) pDevice->Release();
        }
    }

    void hook_opengl()
    {
        if (!opengl_hooked)
        {
            HMODULE library = GetModuleHandle(OpenGL_Hook::DLL_NAME);
            decltype(SwapBuffers)* wglSwapBuffers = nullptr;
            if (library != nullptr)
            {
                wglSwapBuffers = (decltype(wglSwapBuffers))GetProcAddress(library, "wglSwapBuffers");
            }
            if (wglSwapBuffers != nullptr)
            {
                APP_LOGI("Hooked wglSwapBuffers to detect OpenGL\n");

                opengl_hooked = true;

                opengl_hook = OpenGL_Hook::Inst();
                opengl_hook->loadFunctions(wglSwapBuffers);

                HookwglSwapBuffers(wglSwapBuffers);
            }
            else
            {
                APP_LOGW("Failed to Hook wglSwapBuffers to detect OpenGL\n");
            }
        }
    }

    void hook_vulkan()
    {
        if (!vulkan_hooked)
        {
            HMODULE library = GetModuleHandle(Vulkan_Hook::DLL_NAME);
            decltype(::vkCmdExecuteCommands)* vkCmdExecuteCommands = nullptr;
            decltype(::vkCmdDraw)* vkCmdDraw = nullptr;
            decltype(::vkCmdDrawIndexed)* vkCmdDrawIndexed = nullptr;
            if (library != nullptr)
            {
                vkCmdExecuteCommands = (decltype(vkCmdExecuteCommands))GetProcAddress(library, "vkCmdExecuteCommands");
                vkCmdDraw = (decltype(vkCmdDraw))GetProcAddress(library, "vkCmdDraw");
                vkCmdDrawIndexed = (decltype(vkCmdDrawIndexed))GetProcAddress(library, "vkCmdDrawIndexed");
            }
            if (vkCmdExecuteCommands != nullptr && vkCmdDraw != nullptr && vkCmdDrawIndexed != nullptr)
            {
                APP_LOGI("Hooked vkCmdExecuteCommands to detect Vulkan\n");

                vulkan_hooked = true;
                
                vulkan_hook = Vulkan_Hook::Inst();
                vulkan_hook->loadFunctions();
                
                HookvkCmdExecuteCommands(vkCmdExecuteCommands, vkCmdDraw, vkCmdDrawIndexed);
            }
            else
            {
                APP_LOGW("Failed to Hook wglSwapBuffers to detect Vulkan\n");
            }
        }
    }

public:
    Renderer_Hook* detect_renderer(std::chrono::milliseconds timeout)
    {
        std::pair<const char*, void(Renderer_Detector::*)()> libraries[]{
            std::pair<const char*, void(Renderer_Detector::*)()>{  DX12_Hook::DLL_NAME, &Renderer_Detector::hook_dx12},
            std::pair<const char*, void(Renderer_Detector::*)()>{  DX11_Hook::DLL_NAME, &Renderer_Detector::hook_dx11},
            std::pair<const char*, void(Renderer_Detector::*)()>{  DX10_Hook::DLL_NAME, &Renderer_Detector::hook_dx10},
            std::pair<const char*, void(Renderer_Detector::*)()>{   DX9_Hook::DLL_NAME, &Renderer_Detector::hook_dx9},
            std::pair<const char*, void(Renderer_Detector::*)()>{OpenGL_Hook::DLL_NAME, &Renderer_Detector::hook_opengl},
            std::pair<const char*, void(Renderer_Detector::*)()>{Vulkan_Hook::DLL_NAME, &Renderer_Detector::hook_vulkan},
        };

        if (create_hwnd() == nullptr)
        {
            return nullptr;
        }

        auto start_time = std::chrono::steady_clock::now();
        do
        {
            for (auto const& library : libraries)
            {
                if (Library::get_module_handle(library.first) != nullptr)
                {
                    std::lock_guard<std::mutex> lk(renderer_mutex);
                    (this->*library.second)();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
        } while (renderer_hook == nullptr && (timeout.count() == -1 || (std::chrono::steady_clock::now() - start_time) <= timeout));

        destroy_hwnd();

        Renderer_Hook* res = renderer_hook;

        delete instance;
        instance = nullptr;

        return res;
    }
};

Renderer_Detector* Renderer_Detector::instance = nullptr;

#elif defined(__LINUX__)
#include "linux/OpenGLX_Hook.h"

class Renderer_Detector
{
    static Renderer_Detector* instance;
public:
    static Renderer_Detector* Inst()
    {
        if (instance == nullptr)
        {
            instance = new Renderer_Detector();
        }
        return instance;
    }

private:
    std::mutex renderer_mutex;

    Base_Hook hooks;

    decltype(::glXSwapBuffers)* glXSwapBuffers;

    bool openglx_hooked;
    //bool vulkan_hooked;

    OpenGLX_Hook* openglx_hook;
    Renderer_Hook* renderer_hook;

    static void MyglXSwapBuffers(Display* dpy, GLXDrawable drawable)
    {
        Inst()->glXSwapBuffers(dpy, drawable);
		Inst()->hooks.UnhookAll();
        Inst()->renderer_hook = static_cast<Renderer_Hook*>(Inst()->openglx_hook);
        Inst()->openglx_hook = nullptr;
    }

    void HookglXSwapBuffers(decltype(::glXSwapBuffers)* _glXSwapBuffers)
    {
        glXSwapBuffers = _glXSwapBuffers;

        hooks.BeginHook();
        hooks.HookFunc(std::pair<void**, void*>{ (void**)&glXSwapBuffers, (void*)&MyglXSwapBuffers });
        hooks.EndHook();
    }

    void hook_openglx()
    {
        if (!openglx_hooked)
        {
            void* library = dlopen(OpenGLX_Hook::DLL_NAME, RTLD_NOW);
            decltype(::glXSwapBuffers)* glXSwapBuffers = nullptr;
            if (library != nullptr)
            {
                glXSwapBuffers = (decltype(glXSwapBuffers))dlsym(library, "glXSwapBuffers");
            }
            if (glXSwapBuffers != nullptr)
            {
                APP_LOGI("Hooked glXSwapBuffers to detect OpenGLX");

                openglx_hooked = true;

                openglx_hook = OpenGLX_Hook::Inst();
                openglx_hook->loadFunctions(glXSwapBuffers);

                HookglXSwapBuffers(glXSwapBuffers);
            }
            else
            {
                APP_LOGW("Failed to Hook glXSwapBuffers to detect OpenGLX");
            }
        }
    }

public:
    Renderer_Hook* detect_renderer(std::chrono::milliseconds timeout)
    {
        std::pair<const char*, void(Renderer_Detector::*)()> libraries[]{
            std::pair<const char*, void(Renderer_Detector::*)()>{OpenGLX_Hook::DLL_NAME,& Renderer_Detector::hook_openglx},
        };

        auto start_time = std::chrono::steady_clock::now();
        do
        {
            for (auto const& library : libraries)
            {
                if (Library::get_module_handle(library.first) != nullptr)
                {
                    std::lock_guard<std::mutex> lk(renderer_mutex);
                    (this->*library.second)();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
        } while (renderer_hook == nullptr && (timeout.count() == -1 || (std::chrono::steady_clock::now() - start_time) <= timeout));

        Renderer_Hook* res = renderer_hook;

        delete openglx_hook;
        //delete vulkan_hook;

        delete instance;
        instance = nullptr;

        return res;
    }
};

Renderer_Detector* Renderer_Detector::instance = nullptr;

#elif defined(__APPLE__)

class Renderer_Detector
{
    static Renderer_Detector* instance;
public:
    static Renderer_Detector* Inst()
    {
        if (instance == nullptr)
        {
            instance = new Renderer_Detector();
        }
        return instance;
    }

    Renderer_Hook* detect_renderer(std::chrono::milliseconds timeout)
    {
        delete instance;
        instance = nullptr;

        return nullptr;
    }
};

Renderer_Detector* Renderer_Detector::instance = nullptr;

#endif

std::future<Renderer_Hook*> detect_renderer(std::chrono::milliseconds timeout)
{
    return std::async(std::launch::async, &Renderer_Detector::detect_renderer, Renderer_Detector::Inst(), timeout);
}