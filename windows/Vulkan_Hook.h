#pragma once

#include "../Renderer_Hook.h"

#include <vulkan.h>

class Vulkan_Hook : public Renderer_Hook
{
public:
    static constexpr const char *DLL_NAME = "vulkan-1.dll";

private:
    static Vulkan_Hook* _inst;

    // Variables
    bool hooked;
    bool initialized;

    // Functions
    Vulkan_Hook();

    void resetRenderState();
    void prepareForOverlay();

public:
    virtual ~Vulkan_Hook();

    virtual bool start_hook();
    virtual bool is_started();
    static Vulkan_Hook* Inst();
    virtual const char* get_lib_name() const;
    void loadFunctions();

    virtual std::shared_ptr<uint64_t> CreateImageResource(std::shared_ptr<Image> source);
};
