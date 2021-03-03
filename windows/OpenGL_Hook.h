#pragma once

#include "../Renderer_Hook.h"

class OpenGL_Hook : public Renderer_Hook
{
public:
    static constexpr const char *DLL_NAME = "opengl32.dll";

    using wglSwapBuffers_t = BOOL(WINAPI*)(HDC);

private:
    static OpenGL_Hook* _inst;

    // Variables
    bool hooked;
    bool initialized;
    HWND last_window;

    // Functions
    OpenGL_Hook();

    void resetRenderState();
    void prepareForOverlay(HDC hDC);

    // Hook to render functions
    static BOOL WINAPI MywglSwapBuffers(HDC hDC);

    wglSwapBuffers_t wglSwapBuffers;

public:
    virtual ~OpenGL_Hook();

    virtual bool start_hook();
    virtual bool is_started();
    static OpenGL_Hook* Inst();
    virtual const char* get_lib_name() const;
    void loadFunctions(wglSwapBuffers_t pfnwglSwapBuffers);

    virtual std::shared_ptr<uint64_t> CreateImageResource(std::shared_ptr<Image> source);
};
