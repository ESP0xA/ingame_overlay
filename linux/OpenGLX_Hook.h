#pragma once

#include "../Renderer_Hook.h"

#include <GL/glew.h>
#include <GL/glx.h>

class OpenGLX_Hook : public Renderer_Hook
{
public:
    static constexpr const char *DLL_NAME = "libGLX.so";

private:
    static OpenGLX_Hook* _inst;

    // Variables
    bool hooked;
    bool initialized;
    Display *display;
    GLXContext context;

    // Functions
    OpenGLX_Hook();

    void resetRenderState();
    void prepareForOverlay(Display* display, GLXDrawable drawable);

    // Hook to render functions
    decltype(glXSwapBuffers)* _glXSwapBuffers;

public:
    static void MyglXSwapBuffers(Display* display, GLXDrawable drawable);

    virtual ~OpenGLX_Hook();

    virtual bool start_hook();
    virtual bool is_started();
    static OpenGLX_Hook* Inst();
    virtual const char* get_lib_name() const;
    void loadFunctions(decltype(glXSwapBuffers)* pfnglXSwapBuffers);

    virtual std::shared_ptr<uint64_t> CreateImageResource(std::shared_ptr<Image> source);
};
