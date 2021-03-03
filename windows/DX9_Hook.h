#pragma once

#include "../Renderer_Hook.h"

#include <d3d9.h>

class DX9_Hook : public Renderer_Hook
{
public:
    static constexpr const char *DLL_NAME = "d3d9.dll";

private:
    static DX9_Hook* _inst;

    // Variables
    bool hooked;
    bool initialized;
    bool uses_present;
    HWND last_window;
    IDirect3DDevice9* pDevice;

    // Functions
    DX9_Hook();

    void resetRenderState();
    void prepareForOverlay(IDirect3DDevice9* pDevice);

    // Hook to render functions
    decltype(&IDirect3DDevice9::Reset)       Reset;
    decltype(&IDirect3DDevice9::EndScene)    EndScene;
    decltype(&IDirect3DDevice9::Present)     Present;
    decltype(&IDirect3DDevice9Ex::PresentEx) PresentEx;

    static HRESULT STDMETHODCALLTYPE MyReset(IDirect3DDevice9* _this, D3DPRESENT_PARAMETERS* pPresentationParameters);
    static HRESULT STDMETHODCALLTYPE MyEndScene(IDirect3DDevice9 *_this);
    static HRESULT STDMETHODCALLTYPE MyPresent(IDirect3DDevice9* _this, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);
    static HRESULT STDMETHODCALLTYPE MyPresentEx(IDirect3DDevice9Ex* _this, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags);

public:
    virtual ~DX9_Hook();

    virtual bool start_hook();
    virtual bool is_started();
    static DX9_Hook* Inst();
    virtual const char* get_lib_name() const;

    void loadFunctions(decltype(Present) PresentFcn, decltype(Reset) ResetFcn, decltype(EndScene) EndSceneFcn, decltype(PresentEx) PresentExFcn);

    virtual std::shared_ptr<uint64_t> CreateImageResource(std::shared_ptr<Image> source);
};
