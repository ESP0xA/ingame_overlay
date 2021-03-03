#include "OpenGLX_Hook.h"
#include "X11_Hook.h"

#include <imgui.h>
#include <impls/imgui_impl_opengl3.h>

OpenGLX_Hook* OpenGLX_Hook::_inst = nullptr;

bool OpenGLX_Hook::start_hook()
{
    bool res = true;
    if (!hooked)
    {
        if (!X11_Hook::Inst()->start_hook())
            return false;

        GLenum err = glewInit();

        if (err == GLEW_OK)
        {
            APP_LOGI("Hooked OpenGLX\n");

            hooked = true;

            UnhookAll();
            BeginHook();
            HookFuncs(
                std::make_pair<void**, void*>((void**)&_glXSwapBuffers, (void*)&OpenGLX_Hook::MyglXSwapBuffers)
            );
            EndHook();
        }
        else
        {
            APP_LOGW("Failed to hook OpenGLX\n");
            /* Problem: glewInit failed, something is seriously wrong. */
            APP_LOGW("Error: %s\n", glewGetErrorString(err));
            res = false;
        }
    }
    return true;
}

bool OpenGLX_Hook::is_started()
{
    return hooked;
}

void OpenGLX_Hook::resetRenderState()
{
    if (initialized)
    {
        overlay_hook_ready(false);

        ImGui_ImplOpenGL3_Shutdown();
        X11_Hook::Inst()->resetRenderState();
        ImGui::DestroyContext();

        glXDestroyContext(display, context);
        display = nullptr;
        initialized = false;
    }
}

// Try to make this function and overlay's proc as short as possible or it might affect game's fps.
void OpenGLX_Hook::prepareForOverlay(Display* display, GLXDrawable drawable)
{
    APP_LOGT("Called SwapBuffer hook");

    if( !initialized )
    {
        ImGui::CreateContext();
        ImGui_ImplOpenGL3_Init();

        int attributes[] = { //can't be const b/c X11 doesn't like it.  Not sure if that's intentional or just stupid.
            GLX_RGBA, //apparently nothing comes after this?
            GLX_RED_SIZE,    8,
            GLX_GREEN_SIZE,  8,
            GLX_BLUE_SIZE,   8,
            GLX_ALPHA_SIZE,  8,
            //Ideally, the size would be 32 (or at least 24), but I have actually seen
            //  this size (on a modern OS even).
            GLX_DEPTH_SIZE, 16,
            GLX_DOUBLEBUFFER, True,
            None
        };

        XVisualInfo* visual_info = glXChooseVisual(display, DefaultScreen(display), attributes);
        if (visual_info == nullptr)
            return;

        context = glXCreateContext(display, visual_info, nullptr, True);
        if (context == nullptr)
            return;

        this->display = display;

        initialized = true;
        overlay_hook_ready(true);
    }

    auto oldContext = glXGetCurrentContext();

    glXMakeCurrent(display, drawable, context);

    if (ImGui_ImplOpenGL3_NewFrame() && X11_Hook::Inst()->prepareForOverlay(display, (Window)drawable))
    {
        ImGui::NewFrame();

        overlay_proc();

        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    glXMakeCurrent(display, drawable, oldContext);
}

void OpenGLX_Hook::MyglXSwapBuffers(Display* display, GLXDrawable drawable)
{
    OpenGLX_Hook::Inst()->prepareForOverlay(display, drawable);
    OpenGLX_Hook::Inst()->_glXSwapBuffers(display, drawable);
}

OpenGLX_Hook::OpenGLX_Hook():
    initialized(false),
    hooked(false),
    _glXSwapBuffers(nullptr)
{
    //_library = dlopen(DLL_NAME);
}

OpenGLX_Hook::~OpenGLX_Hook()
{
    APP_LOGI("OpenGLX Hook removed\n");

    if (initialized)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
        glXDestroyContext(display, context);
    }

    //dlclose(_library);

    _inst = nullptr;
}

OpenGLX_Hook* OpenGLX_Hook::Inst()
{
    if (_inst == nullptr)
        _inst = new OpenGLX_Hook;

    return _inst;
}

const char* OpenGLX_Hook::get_lib_name() const
{
    return DLL_NAME;
}

void OpenGLX_Hook::loadFunctions(decltype(glXSwapBuffers)* pfnglXSwapBuffers)
{
    _glXSwapBuffers = pfnglXSwapBuffers;
}

std::shared_ptr<uint64_t> OpenGLX_Hook::CreateImageResource(std::shared_ptr<Image> source)
{
    GLuint* texture = new GLuint(0);
    glGenTextures(1, texture);
    if (glGetError() != GL_NO_ERROR)
    {
        delete texture;
        return nullptr;
    }
    
    // Save old texture id
    GLint oldTex;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTex);

    glBindTexture(GL_TEXTURE_2D, *texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, source->width(), source->height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, source->get_raw_pointer());

    glBindTexture(GL_TEXTURE_2D, oldTex);

    return std::shared_ptr<uint64_t>((uint64_t*)texture, [](uint64_t* handle)
    {
        if (handle != nullptr)
        {
            GLuint* texture = (GLuint*)handle;
            glDeleteTextures(1, texture);
            delete texture;
        }
    });
}
