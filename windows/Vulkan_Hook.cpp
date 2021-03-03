#include "Vulkan_Hook.h"
#include "Windows_Hook.h"

#include <imgui.h>
//#include <impls/imgui_impl_opengl3.h>

Vulkan_Hook* Vulkan_Hook::_inst = nullptr;

bool Vulkan_Hook::start_hook()
{
    bool res = true;
    if (!hooked)
    {
        
    }
    return true;
}

bool Vulkan_Hook::is_started()
{
    return hooked;
}

void Vulkan_Hook::resetRenderState()
{
}

// Try to make this function and overlay's proc as short as possible or it might affect game's fps.
void Vulkan_Hook::prepareForOverlay()
{
    
}

Vulkan_Hook::Vulkan_Hook():
    hooked(false),
    initialized(false)
{
    //_library = LoadLibrary(DLL_NAME);
}

Vulkan_Hook::~Vulkan_Hook()
{
    APP_LOGI("Vulkan_Hook Hook removed\n");

    if (initialized)
    {
    }

    //FreeLibrary(reinterpret_cast<HMODULE>(_library));

    _inst = nullptr;
}

Vulkan_Hook* Vulkan_Hook::Inst()
{
    if (_inst == nullptr)
        _inst = new Vulkan_Hook;

    return _inst;
}

const char* Vulkan_Hook::get_lib_name() const
{
    return DLL_NAME;
}

void Vulkan_Hook::loadFunctions()
{
}

std::shared_ptr<uint64_t> Vulkan_Hook::CreateImageResource(std::shared_ptr<Image> source)
{
    return std::shared_ptr<uint64_t>(nullptr);
}