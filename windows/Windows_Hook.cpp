#include "Windows_Hook.h"

#include <imgui.h>
#include <impls/windows/imgui_impl_win32.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Windows_Hook* Windows_Hook::_inst = nullptr;

bool Windows_Hook::start_hook()
{
    bool res = true;
    if (!hooked)
    {
        GetRawInputBuffer = ::GetRawInputBuffer;
        GetRawInputData   = ::GetRawInputData;
        GetKeyState       = ::GetKeyState;
        GetAsyncKeyState  = ::GetAsyncKeyState;
        GetKeyboardState  = ::GetKeyboardState;
        GetCursorPos      = ::GetCursorPos;

        APP_LOGI("Hooked Windows\n");

        BeginHook();
        HookFuncs(
            std::make_pair<void**, void*>(&(PVOID&)GetRawInputBuffer, &Windows_Hook::MyGetRawInputBuffer),
            std::make_pair<void**, void*>(&(PVOID&)GetRawInputData  , &Windows_Hook::MyGetRawInputData),
            std::make_pair<void**, void*>(&(PVOID&)GetKeyState      , &Windows_Hook::MyGetKeyState),
            std::make_pair<void**, void*>(&(PVOID&)GetAsyncKeyState , &Windows_Hook::MyGetAsyncKeyState),
            std::make_pair<void**, void*>(&(PVOID&)GetKeyboardState , &Windows_Hook::MyGetKeyboardState),
            std::make_pair<void**, void*>(&(PVOID&)GetCursorPos     , &Windows_Hook::MyGetCursorPos)
        );
        EndHook();

        hooked = true;
    }
    return res;
}

void Windows_Hook::resetRenderState()
{
    if (initialized)
    {
        initialized = false;
        SetWindowLongPtr(_game_hwnd, GWLP_WNDPROC, (LONG_PTR)_game_wndproc);
        _game_hwnd = nullptr;
        _game_wndproc = nullptr;
        ImGui_ImplWin32_Shutdown();
    }
}

bool Windows_Hook::prepareForOverlay(HWND hWnd)
{
    if (_game_hwnd != hWnd)
        resetRenderState();

    if (!initialized)
    {
        _game_hwnd = hWnd;
        ImGui_ImplWin32_Init(_game_hwnd);

        initialized = true;
    }

    if (initialized)
    {
        void* current_proc = (void*)GetWindowLongPtr(_game_hwnd, GWLP_WNDPROC);
        if (current_proc == nullptr)
            return false;

        if (current_proc != &Windows_Hook::HookWndProc)
        {
            _game_wndproc = (WNDPROC)SetWindowLongPtr(_game_hwnd, GWLP_WNDPROC, (LONG_PTR)&Windows_Hook::HookWndProc);
        }

        ImGui_ImplWin32_NewFrame();
        // Read keyboard modifiers inputs
        auto& io = ImGui::GetIO();
        io.KeyCtrl = (this->GetKeyState(VK_CONTROL) & 0x8000) != 0;
        io.KeyShift = (this->GetKeyState(VK_SHIFT) & 0x8000) != 0;
        io.KeyAlt = (this->GetKeyState(VK_MENU) & 0x8000) != 0;
        return true;
    }

    return false;
}

HWND Windows_Hook::GetGameHwnd() const
{
    return _game_hwnd;
}

WNDPROC Windows_Hook::GetGameWndProc() const
{
    return _game_wndproc;
}

/////////////////////////////////////////////////////////////////////////////////////
// Windows window hooks
bool IgnoreMsg(UINT uMsg)
{
    switch (uMsg)
    {
        // Mouse Events
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        case WM_LBUTTONUP: case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONUP: case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONUP: case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONUP: case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        case WM_MOUSEACTIVATE: case WM_MOUSEHOVER: case WM_MOUSELEAVE:
        // Keyboard Events
        case WM_KEYDOWN: case WM_KEYUP:
        case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_SYSDEADCHAR:
        case WM_CHAR: case WM_UNICHAR: case WM_DEADCHAR:
        // Raw Input Events
        case WM_INPUT:
            return true;
    }
    return false;
}

void RawEvent(RAWINPUT& raw)
{
    if (raw.header.dwType == RIM_TYPEMOUSE)
    {
        if (raw.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
            ImGui_ImplWin32_WndProcHandler(Windows_Hook::Inst()->GetGameHwnd(), WM_LBUTTONDOWN, 0, 0);
        else if (raw.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
            ImGui_ImplWin32_WndProcHandler(Windows_Hook::Inst()->GetGameHwnd(), WM_LBUTTONUP, 0, 0);
        else if (raw.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
            ImGui_ImplWin32_WndProcHandler(Windows_Hook::Inst()->GetGameHwnd(), WM_MBUTTONDOWN, 0, 0);
        else if (raw.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
            ImGui_ImplWin32_WndProcHandler(Windows_Hook::Inst()->GetGameHwnd(), WM_MBUTTONUP, 0, 0);
        else if (raw.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
            ImGui_ImplWin32_WndProcHandler(Windows_Hook::Inst()->GetGameHwnd(), WM_RBUTTONDOWN, 0, 0);
        else if (raw.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
            ImGui_ImplWin32_WndProcHandler(Windows_Hook::Inst()->GetGameHwnd(), WM_RBUTTONUP, 0, 0);
        else if (raw.data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
            ImGui_ImplWin32_WndProcHandler(Windows_Hook::Inst()->GetGameHwnd(), WM_MOUSEWHEEL, ((WPARAM)raw.data.mouse.usButtonData) << 16, 0);
        else if (raw.data.mouse.usButtonFlags & RI_MOUSE_HWHEEL)
            ImGui_ImplWin32_WndProcHandler(Windows_Hook::Inst()->GetGameHwnd(), WM_MOUSEHWHEEL, ((WPARAM)raw.data.mouse.usButtonData) << 16, 0);
    }
    else if (raw.header.dwType == RIM_TYPEKEYBOARD)
    {
        //raw.data.keyboard.
    }
}

LRESULT CALLBACK Windows_Hook::HookWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Windows_Hook* _this = Windows_Hook::Inst();
    if (_this->initialized)
    {
        auto& overlay = Get_Overlay();
        if (overlay.Ready())
        {
            ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
            POINT pos;
            if (_this->GetCursorPos(&pos) && ScreenToClient(hWnd, &pos))
            {
                ImGui::GetIO().MousePos = ImVec2((float)pos.x, (float)pos.y);
            }

            bool ignore_message = overlay.ShowOverlay();
            // Is the event is a key press
            if (uMsg == WM_KEYDOWN)
            {
                // Tab is pressed and was not pressed before
                if (wParam == VK_TAB && !(lParam & (1 << 30)))
                {
                    // If Left Shift is pressed
                    if (_this->GetAsyncKeyState(VK_LSHIFT) & (1 << 15))
                    {
                        ignore_message = true;
                        overlay.ShowOverlay(!overlay.ShowOverlay());
                    }
                }
            }

            if (ignore_message)
            {
                if (IgnoreMsg(uMsg))
                {
                    return 0;
                }
            }
        }
    }
    // Call the overlay window procedure
    return CallWindowProc(Windows_Hook::Inst()->_game_wndproc, hWnd, uMsg, wParam, lParam);
}

UINT WINAPI Windows_Hook::MyGetRawInputBuffer(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader)
{
    Windows_Hook* _this = Windows_Hook::Inst();
    int res = _this->GetRawInputBuffer(pData, pcbSize, cbSizeHeader);
    if (!_this->initialized)
        return res;

    if (pData != nullptr)
    {
        for (int i = 0; i < res; ++i)
            RawEvent(pData[i]);
    }

    auto& overlay = Get_Overlay();
    if (!overlay.ShowOverlay())
        return res;

    return 0;
}

UINT WINAPI Windows_Hook::MyGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader)
{
    Windows_Hook* _this = Windows_Hook::Inst();
    auto res = _this->GetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
    if (!_this->initialized)
        return res;

    if (pData != nullptr && uiCommand == RID_INPUT && res == sizeof(RAWINPUT))
        RawEvent(*reinterpret_cast<RAWINPUT*>(pData));

    auto& overlay = Get_Overlay();
    if (!overlay.ShowOverlay())
        return res;

    *pcbSize = 0;
    return 0;
}

SHORT WINAPI Windows_Hook::MyGetKeyState(int nVirtKey)
{
    Windows_Hook* _this = Windows_Hook::Inst();
    auto res = _this->GetKeyState(nVirtKey);
    if (!_this->initialized)
        return res;

    auto& overlay = Get_Overlay();
    if (!overlay.ShowOverlay())
        return res;

    return 0;
}

SHORT WINAPI Windows_Hook::MyGetAsyncKeyState(int vKey)
{
    Windows_Hook* _this = Windows_Hook::Inst();
    auto res = _this->GetAsyncKeyState(vKey);
    if (!_this->initialized)
        return res;
    
    auto& overlay = Get_Overlay();
    if (!overlay.ShowOverlay())
        return res;

    return 0;
}

BOOL WINAPI Windows_Hook::MyGetKeyboardState(PBYTE lpKeyState)
{
    Windows_Hook* _this = Windows_Hook::Inst();
    auto res = _this->GetKeyboardState(lpKeyState);
    if (!_this->initialized)
        return res;

    auto& overlay = Get_Overlay();
    if (!overlay.ShowOverlay())
        return res;

    return 0;
}

BOOL  WINAPI Windows_Hook::MyGetCursorPos(LPPOINT lpPoint)
{
    Windows_Hook* _this = Windows_Hook::Inst();
    auto res = _this->GetCursorPos(lpPoint);
    if (!_this->initialized)
        return res;

    auto& overlay = Get_Overlay();
    if (!overlay.ShowOverlay())
        return res;

    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////

Windows_Hook::Windows_Hook() :
    initialized(false),
    hooked(false),
    _game_hwnd(nullptr),
    _game_wndproc(nullptr),
    GetRawInputBuffer(nullptr),
    GetRawInputData(nullptr),
    GetKeyState(nullptr),
    GetAsyncKeyState(nullptr),
    GetKeyboardState(nullptr)
{
    //_library = LoadLibrary(DLL_NAME);
}

Windows_Hook::~Windows_Hook()
{
    APP_LOGI("Windows Hook removed\n");

    resetRenderState();

    //FreeLibrary(reinterpret_cast<HMODULE>(_library));

    _inst = nullptr;
}

Windows_Hook* Windows_Hook::Inst()
{
    if (_inst == nullptr)
        _inst = new Windows_Hook;

    return _inst;
}

const char* Windows_Hook::get_lib_name() const
{
    return DLL_NAME;
}
