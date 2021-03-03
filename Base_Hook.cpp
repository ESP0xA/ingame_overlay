#include "Base_Hook.h"
#include "Hook_Manager.h"

#include <algorithm>
#include <mini_detour.h>

Base_Hook::Base_Hook():
    _library(nullptr)
{}

Base_Hook::~Base_Hook()
{
    UnhookAll();
}

const char* Base_Hook::get_lib_name() const
{
    return "<no_name>";
}

void Base_Hook::BeginHook()
{
    //mini_detour::transaction_begin();
}

void Base_Hook::EndHook()
{
    //mini_detour::transaction_commit();
}

void Base_Hook::HookFunc(std::pair<void**, void*> hook)
{
    mini_detour::hook md_hook;
    void* res = md_hook.hook_func(*hook.first, hook.second);
    if (res != nullptr)
    {
        _hooked_funcs.emplace_back(std::move(md_hook));
        *hook.first = res;
    }
}

void Base_Hook::UnhookAll()
{
    if (_hooked_funcs.size())
    {
        _hooked_funcs.clear();
    }
}