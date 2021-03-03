#include "DX9_Hook.h"
#include "Windows_Hook.h"
#include "DirectX_VTables.h"

#include <imgui.h>
#include <impls/windows/imgui_impl_dx9.h>

DX9_Hook* DX9_Hook::_inst = nullptr;

bool DX9_Hook::start_hook()
{
    if (!hooked)
    {
        if (!Windows_Hook::Inst()->start_hook())
            return false;

        APP_LOGI("Hooked DirectX 9\n");
        hooked = true;

        BeginHook();
        HookFuncs(
            std::make_pair<void**, void*>(&(PVOID&)Reset, &DX9_Hook::MyReset),
            std::make_pair<void**, void*>(&(PVOID&)Present, &DX9_Hook::MyPresent)
        );
        if (PresentEx != nullptr)
        {
            HookFuncs(
                std::make_pair<void**, void*>(&(PVOID&)PresentEx, &DX9_Hook::MyPresentEx)
                //std::make_pair<void**, void*>(&(PVOID&)EndScene, &DX9_Hook::MyEndScene)
            );
        }
        EndHook();
    }
    return true;
}

bool DX9_Hook::is_started()
{
    return hooked;
}

void DX9_Hook::resetRenderState()
{
    if (initialized)
    {
        overlay_hook_ready(false);

        ImGui_ImplDX9_Shutdown();
        Windows_Hook::Inst()->resetRenderState();
        ImGui::DestroyContext();

        pDevice->Release();
        pDevice = nullptr;
        last_window = nullptr;
        initialized = false;
    }
}

// Try to make this function and overlay's proc as short as possible or it might affect game's fps.
void DX9_Hook::prepareForOverlay(IDirect3DDevice9 *pDevice)
{
    D3DDEVICE_CREATION_PARAMETERS param;
    pDevice->GetCreationParameters(&param);

    // Workaround to detect if we changed window.
    if (param.hFocusWindow == last_window || this->pDevice != pDevice)
        resetRenderState();

    if (!initialized)
    {
        pDevice->AddRef();
        this->pDevice = pDevice;

        ImGui::CreateContext();
        ImGui_ImplDX9_Init(pDevice);

        last_window = param.hFocusWindow;
        initialized = true;
        overlay_hook_ready(true);
    }

    if (ImGui_ImplDX9_NewFrame() && Windows_Hook::Inst()->prepareForOverlay(param.hFocusWindow))
    {
        ImGui::NewFrame();

        overlay_proc();

        ImGui::Render();

        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
}

HRESULT STDMETHODCALLTYPE DX9_Hook::MyReset(IDirect3DDevice9* _this, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    DX9_Hook::Inst()->resetRenderState();
    return (_this->*DX9_Hook::Inst()->Reset)(pPresentationParameters);
}

HRESULT STDMETHODCALLTYPE DX9_Hook::MyEndScene(IDirect3DDevice9* _this)
{   
    if( !DX9_Hook::Inst()->uses_present )
        DX9_Hook::Inst()->prepareForOverlay(_this);
    return (_this->*DX9_Hook::Inst()->EndScene)();
}

HRESULT STDMETHODCALLTYPE DX9_Hook::MyPresent(IDirect3DDevice9* _this, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
    DX9_Hook::Inst()->uses_present = true;
    DX9_Hook::Inst()->prepareForOverlay(_this);
    return (_this->*DX9_Hook::Inst()->Present)(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT STDMETHODCALLTYPE DX9_Hook::MyPresentEx(IDirect3DDevice9Ex* _this, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
{
    DX9_Hook::Inst()->uses_present = true;
    DX9_Hook::Inst()->prepareForOverlay(_this);
    return (_this->*DX9_Hook::Inst()->PresentEx)(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

DX9_Hook::DX9_Hook():
    initialized(false),
    hooked(false),
    uses_present(false),
    last_window(nullptr),
    EndScene(nullptr),
    Present(nullptr),
    PresentEx(nullptr),
    Reset(nullptr)
{
    _library = LoadLibrary(DLL_NAME);
}

DX9_Hook::~DX9_Hook()
{
    APP_LOGI("DX9 Hook removed\n");

    if (initialized)
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        ImGui::DestroyContext();
    }

    FreeLibrary(reinterpret_cast<HMODULE>(_library));

    _inst = nullptr;
}

DX9_Hook* DX9_Hook::Inst()
{
    if( _inst == nullptr )
        _inst = new DX9_Hook;

    return _inst;
}

const char* DX9_Hook::get_lib_name() const
{
    return DLL_NAME;
}

void DX9_Hook::loadFunctions(decltype(Present) PresentFcn, decltype(Reset) ResetFcn, decltype(EndScene) EndSceneFcn, decltype(PresentEx) PresentExFcn)
{
    Present = PresentFcn;
    Reset = ResetFcn;
    EndScene = EndSceneFcn;

    PresentEx = PresentExFcn;
}

std::shared_ptr<uint64_t> DX9_Hook::CreateImageResource(std::shared_ptr<Image> source)
{
    IDirect3DTexture9** pTexture = new IDirect3DTexture9*(nullptr);

    pDevice->CreateTexture(
        source->width(),
        source->height(),
        1,
        0,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        pTexture,
        nullptr
    );

    if (*pTexture != nullptr)
    {
        D3DLOCKED_RECT rect;
        if (SUCCEEDED((*pTexture)->LockRect(0, &rect, nullptr, D3DLOCK_DISCARD)))
        {
            uint32_t const* pixels = reinterpret_cast<uint32_t const*>(source->get_raw_pointer());
            uint32_t* texture_bits = reinterpret_cast<uint32_t*>(rect.pBits);
            for (int32_t i = 0; i < source->height(); ++i)
            {
                for (int32_t j = 0; j < source->width(); ++j)
                {
                    // RGBA to ARGB Conversion, DX9 doesn't have a RGBA loader
                    *texture_bits = ((*pixels) << 8) | (*pixels & 0xFF000000) >> 24;

                    ++texture_bits;
                    ++pixels;
                }
            }

            if (FAILED((*pTexture)->UnlockRect(0)))
            {
                (*pTexture)->Release();
                delete pTexture;
                pTexture = nullptr;
            }
        }
        else
        {
            (*pTexture)->Release();
            delete pTexture;
            pTexture = nullptr;
        }
    }

    return std::shared_ptr<uint64_t>((uint64_t*)pTexture, [](uint64_t* handle)
    {
        if (handle != nullptr)
        {
            IDirect3DTexture9** resource = reinterpret_cast<IDirect3DTexture9**>(handle);
            (*resource)->Release();
            delete resource;
        }
    });
}