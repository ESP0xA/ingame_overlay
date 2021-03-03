#pragma once

#include "../Base_Hook.h"

#include <X11/X.h> // XEvent types
#include <X11/Xlib.h> // XEvent structure
#include <X11/Xutil.h> // XEvent keysym

class X11_Hook : public Base_Hook
{
public:
    static constexpr const char* DLL_NAME = "libX11.so";

private:
    static X11_Hook* _inst;

    // Variables
    bool hooked;
    bool initialized;
    Window game_wnd;

    // Functions
    X11_Hook();
    int check_for_overlay(Display *d, int num_events);

    // Hook to X11 window messages
    decltype(XEventsQueued)* _XEventsQueued;
    decltype(XPending)* _XPending;

    static int MyXEventsQueued(Display * display, int mode);
    static int MyXPending(Display* display);

public:
    virtual ~X11_Hook();

    void resetRenderState();
    bool prepareForOverlay(Display *display, Window wnd);

    Window get_game_wnd() const{ return game_wnd; }

    bool start_hook();
    static X11_Hook* Inst();
    virtual const char* get_lib_name() const;
    void loadFunctions(decltype(XEventsQueued)* pfnXEventsQueued, decltype(XPending)* pfnXPending);

};