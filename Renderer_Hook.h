#pragma once

#include "Base_Hook.h"

class Renderer_Hook : public Base_Hook
{
public:
    Renderer_Hook():
        overlay_proc(&default_overlay_proc),
        overlay_hook_ready(&default_overlay_hook_ready)
    {}

    using overlay_proc_t = void();
    using overlay_hook_ready_t = void(bool);

    static void default_overlay_proc() {}
    static void default_overlay_hook_ready(bool) {}
    std::function<overlay_proc_t> overlay_proc;
    std::function<overlay_hook_ready_t> overlay_hook_ready;

    virtual bool start_hook() = 0;
    virtual bool is_started() = 0;
    // Returns a Handle to the renderer image ressource or nullptr if it failed to create the resource, the handle can be used in ImGui's Image calls, image_buffer must be RGBA ordered
    virtual std::shared_ptr<uint64_t> CreateImageResource(std::shared_ptr<Image> source) = 0;
};
