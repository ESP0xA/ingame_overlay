#include "X11_Hook.h"

#include <imgui.h>
#include <impls/linux/imgui_impl_x11.h>

#include <dlfcn.h>
#include <unistd.h>

extern int ImGui_ImplX11_EventHandler(XEvent &event);

X11_Hook* X11_Hook::_inst = nullptr;

bool X11_Hook::start_hook()
{
    bool res = false;

    if (!hooked)
    {
        void* library = Library::get_module_handle(DLL_NAME);

        if (library != nullptr)
        {
            _XEventsQueued = (decltype(_XEventsQueued))dlsym(library, "XEventsQueued");
            _XPending      = (decltype(_XPending))dlsym(library, "XPending");
        }
        else
        {
            APP_LOGW("Failed to load %s to hook X11", DLL_NAME);
        }

        if (_XPending != nullptr && _XEventsQueued != nullptr)
        {
            APP_LOGI("Hooked X11\n");

            hooked = true;
            res = true;

            UnhookAll();
            BeginHook();
            HookFuncs(
                std::make_pair<void**, void*>(&(void*&)_XEventsQueued, (void*)&X11_Hook::MyXEventsQueued),
                std::make_pair<void**, void*>(&(void*&)_XPending, (void*)&X11_Hook::MyXPending)
            );
            EndHook();
        }
    }
    return res;
}

void X11_Hook::resetRenderState()
{
    if (initialized)
    {
        game_wnd = 0;
        initialized = false;
        ImGui_ImplX11_Shutdown();
    }
}

bool X11_Hook::prepareForOverlay(Display *display, Window wnd)
{
    if(!hooked)
        return false;

    if (game_wnd != wnd)
        resetRenderState();

    if (!initialized)
    {
        ImGui_ImplX11_Init(display, (void*)wnd);
        game_wnd = wnd;

        initialized = true;
    }

    ImGui_ImplX11_NewFrame();

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
// X11 window hooks
bool IgnoreEvent(XEvent &event)
{
    switch(event.type)
    {
        // Keyboard
        case KeyPress: case KeyRelease:
        // MouseButton
        case ButtonPress: case ButtonRelease:
        // Mouse move
        case MotionNotify:
            return true;
    }
    return false;
}

int X11_Hook::check_for_overlay(Display *d, int num_events)
{
    static Time prev_time = {};
    auto& overlay = Get_Overlay();
    if (!overlay.Ready())
        return num_events;

    X11_Hook* inst = Inst();

    if( inst->initialized )
    {
        XEvent event;
        while(num_events)
        {
            XPeekEvent(d, &event);
            ImGui_ImplX11_EventHandler(event);

            bool ignore_message = overlay.ShowOverlay();
            // Is the event is a key press
            if (event.type == KeyPress)
            {
                // Tab is pressed and was not pressed before
                if (event.xkey.keycode == XKeysymToKeycode(d, XK_Tab) && event.xkey.state & ShiftMask)
                {
                    // if key TAB is held, don't make the overlay flicker :p
                    if( event.xkey.time != prev_time)
                    {
                        overlay.ShowOverlay(!overlay.ShowOverlay());
                        ignore_message = true;
                    }
                }
            }
            else if(event.type == KeyRelease && event.xkey.keycode == XKeysymToKeycode(d, XK_Tab))
            {
                prev_time = event.xkey.time;
            }

            if (!ignore_message || !IgnoreEvent(event))
            {
                break;
            }

            XNextEvent(d, &event);
            --num_events;
        }
    }
    return num_events;
}

int X11_Hook::MyXEventsQueued(Display *display, int mode)
{
    X11_Hook* inst = X11_Hook::Inst();

    int res = inst->_XEventsQueued(display, mode);

    if( res )
    {
        res = inst->check_for_overlay(display, res);
    }

    return res;
}

int X11_Hook::MyXPending(Display* display)
{
    int res = Inst()->_XPending(display);

    if( res )
    {
        res = Inst()->check_for_overlay(display, res);
    }

    return res;
}

/////////////////////////////////////////////////////////////////////////////////////

X11_Hook::X11_Hook() :
    initialized(false),
    hooked(false),
    game_wnd(0),
    _XEventsQueued(nullptr),
    _XPending(nullptr)
{
    //_library = dlopen(DLL_NAME, RTLD_NOW);
}

X11_Hook::~X11_Hook()
{
    APP_LOGI("X11 Hook removed\n");

    resetRenderState();

    //dlclose(_library);

    _inst = nullptr;
}

X11_Hook* X11_Hook::Inst()
{
    if (_inst == nullptr)
        _inst = new X11_Hook;

    return _inst;
}

const char* X11_Hook::get_lib_name() const
{
    return DLL_NAME;
}
