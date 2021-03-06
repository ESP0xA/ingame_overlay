#pragma once

#include "../Base_Hook.h"

class Windows_Hook : public Base_Hook
{
public:
    static constexpr const char* DLL_NAME = "user32.dll";

private:
    static Windows_Hook* _inst;

    // Variables
    bool hooked;
    bool initialized;
    HWND _game_hwnd;
    WNDPROC _game_wndproc;

    // Functions
    Windows_Hook();

    // Hook to Windows window messages
    decltype(GetRawInputBuffer)* GetRawInputBuffer;
    decltype(GetRawInputData)* GetRawInputData;
    decltype(GetKeyState)* GetKeyState;
    decltype(GetAsyncKeyState)* GetAsyncKeyState;
    decltype(GetKeyboardState)* GetKeyboardState;
    decltype(GetCursorPos)* GetCursorPos;

    static LRESULT CALLBACK HookWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static UINT  WINAPI MyGetRawInputBuffer(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader);
    static UINT  WINAPI MyGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);
    static SHORT WINAPI MyGetKeyState(int nVirtKey);
    static SHORT WINAPI MyGetAsyncKeyState(int vKey);
    static BOOL  WINAPI MyGetKeyboardState(PBYTE lpKeyState);
    static BOOL  WINAPI MyGetCursorPos(LPPOINT lpPoint);

public:
    virtual ~Windows_Hook();

    void resetRenderState();
    bool prepareForOverlay(HWND hWnd);

    HWND GetGameHwnd() const;
    WNDPROC GetGameWndProc() const;

    bool start_hook();
    static Windows_Hook* Inst();
    virtual const char* get_lib_name() const;
};
