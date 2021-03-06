#include "DX10_Hook.h"
#include "Windows_Hook.h"

#include <imgui.h>
#include <impls/windows/imgui_impl_dx10.h>

DX10_Hook* DX10_Hook::_inst = nullptr;

bool DX10_Hook::start_hook()
{
    bool res = true;
    if (!hooked)
    {
        if (!Windows_Hook::Inst()->start_hook())
            return false;

        APP_LOGI("Hooked DirectX 10\n");
        hooked = true;

        BeginHook();
        HookFuncs(
            std::make_pair<void**, void*>(&(PVOID&)DX10_Hook::Present, &DX10_Hook::MyPresent),
            std::make_pair<void**, void*>(&(PVOID&)DX10_Hook::ResizeTarget, &DX10_Hook::MyResizeTarget),
            std::make_pair<void**, void*>(&(PVOID&)DX10_Hook::ResizeBuffers, &DX10_Hook::MyResizeBuffers)
        );
        EndHook();
    }
    return res;
}

bool DX10_Hook::is_started()
{
    return hooked;
}

void DX10_Hook::resetRenderState()
{
    if (initialized)
    {
        overlay_hook_ready(false);

        ImGui_ImplDX10_Shutdown();
        Windows_Hook::Inst()->resetRenderState();
        ImGui::DestroyContext();

        mainRenderTargetView->Release();
        pDevice->Release();

        initialized = false;
    }
}

// Try to make this function and overlay's proc as short as possible or it might affect game's fps.
void DX10_Hook::prepareForOverlay(IDXGISwapChain* pSwapChain)
{
    DXGI_SWAP_CHAIN_DESC desc;
    pSwapChain->GetDesc(&desc);

    if (!initialized)
    {
        if (FAILED(pSwapChain->GetDevice(IID_PPV_ARGS(&pDevice))))
            return;

        ID3D10Texture2D* pBackBuffer = nullptr;

        pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (pBackBuffer == nullptr)
        {
            pDevice->Release();
            return;
        }

        pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
        pBackBuffer->Release();

        ImGui::CreateContext();
        ImGui_ImplDX10_Init(pDevice);

        initialized = true;
        overlay_hook_ready(true);
    }

    if (ImGui_ImplDX10_NewFrame() && Windows_Hook::Inst()->prepareForOverlay(desc.OutputWindow))
    {
        ImGui::NewFrame();

        overlay_proc();

        ImGui::Render();

        pDevice->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
        ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());
    }
}

HRESULT STDMETHODCALLTYPE DX10_Hook::MyPresent(IDXGISwapChain *_this, UINT SyncInterval, UINT Flags)
{
    DX10_Hook::Inst()->prepareForOverlay(_this);
    return (_this->*DX10_Hook::Inst()->Present)(SyncInterval, Flags);
}

HRESULT STDMETHODCALLTYPE DX10_Hook::MyResizeTarget(IDXGISwapChain* _this, const DXGI_MODE_DESC* pNewTargetParameters)
{
    DX10_Hook::Inst()->resetRenderState();
    return (_this->*DX10_Hook::Inst()->ResizeTarget)(pNewTargetParameters);
}

HRESULT STDMETHODCALLTYPE DX10_Hook::MyResizeBuffers(IDXGISwapChain* _this, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    DX10_Hook::Inst()->resetRenderState();
    return (_this->*DX10_Hook::Inst()->ResizeBuffers)(BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

DX10_Hook::DX10_Hook():
    initialized(false),
    hooked(false),
    pDevice(nullptr),
    mainRenderTargetView(nullptr),
    Present(nullptr),
    ResizeBuffers(nullptr),
    ResizeTarget(nullptr)
{
    _library = LoadLibrary(DLL_NAME);
}

DX10_Hook::~DX10_Hook()
{
    APP_LOGI("DX10 Hook removed\n");

    if (initialized)
    {
        mainRenderTargetView->Release();

        ImGui_ImplDX10_InvalidateDeviceObjects();
        ImGui::DestroyContext();

        initialized = false;
    }

    FreeLibrary(reinterpret_cast<HMODULE>(_library));

    _inst = nullptr;
}

DX10_Hook* DX10_Hook::Inst()
{
    if (_inst == nullptr)   
        _inst = new DX10_Hook;

    return _inst;
}

const char* DX10_Hook::get_lib_name() const
{
    return DLL_NAME;
}

void DX10_Hook::loadFunctions(decltype(Present) PresentFcn, decltype(ResizeBuffers) ResizeBuffersFcn, decltype(ResizeTarget) ResizeTargetFcn)
{
    Present = PresentFcn;
    ResizeBuffers = ResizeBuffersFcn;
    ResizeTarget = ResizeTargetFcn;
}

std::shared_ptr<uint64_t> DX10_Hook::CreateImageResource(std::shared_ptr<Image> source)
{
    ID3D10ShaderResourceView** resource = new ID3D10ShaderResourceView*(nullptr);

    // Create texture
    D3D10_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(source->width());
    desc.Height = static_cast<UINT>(source->height());
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D10_USAGE_DEFAULT;
    desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D10Texture2D* pTexture = nullptr;
    D3D10_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = source->get_raw_pointer();
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    pDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    if (pTexture != nullptr)
    {
        // Create texture view
        D3D10_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;

        pDevice->CreateShaderResourceView(pTexture, &srvDesc, resource);
        // Release Texure, the shader resource increases the reference count.
        pTexture->Release();
    }

    return std::shared_ptr<uint64_t>((uint64_t*)resource, [](uint64_t* handle)
    {
        if (handle != nullptr)
        {
            ID3D10ShaderResourceView** resource = reinterpret_cast<ID3D10ShaderResourceView**>(handle);
            (*resource)->Release();
            delete resource;
        }
    });
}