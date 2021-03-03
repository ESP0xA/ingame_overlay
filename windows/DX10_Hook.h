#pragma once

#include "../Renderer_Hook.h"

#include <d3d10.h>

class DX10_Hook : public Renderer_Hook
{
public:
    static constexpr const char *DLL_NAME = "d3d10.dll";

private:
    static DX10_Hook* _inst;

    // Variables
    bool hooked;
    bool initialized;
    ID3D10Device* pDevice;
    ID3D10RenderTargetView* mainRenderTargetView;

    // Functions
    DX10_Hook();

    void resetRenderState();
    void prepareForOverlay(IDXGISwapChain *pSwapChain);

    // Hook to render functions
    static HRESULT STDMETHODCALLTYPE MyPresent(IDXGISwapChain* _this, UINT SyncInterval, UINT Flags);
    static HRESULT STDMETHODCALLTYPE MyResizeTarget(IDXGISwapChain* _this, const DXGI_MODE_DESC* pNewTargetParameters);
    static HRESULT STDMETHODCALLTYPE MyResizeBuffers(IDXGISwapChain* _this, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

    decltype(&IDXGISwapChain::Present)       Present;
    decltype(&IDXGISwapChain::ResizeBuffers) ResizeBuffers;
    decltype(&IDXGISwapChain::ResizeTarget)  ResizeTarget;

public:
    virtual ~DX10_Hook();

    virtual bool start_hook();
    virtual bool is_started();
    static DX10_Hook* Inst();
    virtual const char* get_lib_name() const;

    void loadFunctions(decltype(Present) PresentFcn, decltype(ResizeBuffers) ResizeBuffersFcn, decltype(ResizeTarget) ResizeTargetFcn);

    virtual std::shared_ptr<uint64_t> CreateImageResource(std::shared_ptr<Image> source);
};
