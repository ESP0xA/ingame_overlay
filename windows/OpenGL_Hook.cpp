#include "OpenGL_Hook.h"
#include "Windows_Hook.h"

#include <imgui.h>
#include <impls/imgui_impl_opengl3.h>

#include <GL/glew.h>

OpenGL_Hook* OpenGL_Hook::_inst = nullptr;

bool OpenGL_Hook::start_hook()
{
    bool res = true;
    if (!hooked)
    {
        if (!Windows_Hook::Inst()->start_hook())
            return false;

        GLenum err = glewInit();

        if (err == GLEW_OK)
        {
            APP_LOGI("Hooked OpenGL\n");

            hooked = true;

            UnhookAll();
            BeginHook();
            HookFuncs(
                std::make_pair<void**, void*>(&(PVOID&)wglSwapBuffers, &OpenGL_Hook::MywglSwapBuffers)
            );
            EndHook();
        }
        else
        {
            APP_LOGW("Failed to hook OpenGL\n");
            /* Problem: glewInit failed, something is seriously wrong. */
            APP_LOGW("Error: {}\n", glewGetErrorString(err));
            res = false;
        }
    }
    return true;
}

bool OpenGL_Hook::is_started()
{
    return hooked;
}

void OpenGL_Hook::resetRenderState()
{
    if (initialized)
    {
        overlay_hook_ready(false);

        ImGui_ImplOpenGL3_Shutdown();
        Windows_Hook::Inst()->resetRenderState();
        ImGui::DestroyContext();

        last_window = nullptr;
        initialized = false;
    }
}

// Try to make this function and overlay's proc as short as possible or it might affect game's fps.
void OpenGL_Hook::prepareForOverlay(HDC hDC)
{
    HWND hWnd = WindowFromDC(hDC);

    if (hWnd != last_window)
        resetRenderState();

    if (!initialized)
    {
        ImGui::CreateContext();
        ImGui_ImplOpenGL3_Init();

        last_window = hWnd;
        initialized = true;
        overlay_hook_ready(true);
    }

    if (ImGui_ImplOpenGL3_NewFrame() && Windows_Hook::Inst()->prepareForOverlay(hWnd))
    {
        ImGui::NewFrame();

        overlay_proc();

        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

BOOL WINAPI OpenGL_Hook::MywglSwapBuffers(HDC hDC)
{
    OpenGL_Hook::Inst()->prepareForOverlay(hDC);
    return OpenGL_Hook::Inst()->wglSwapBuffers(hDC);
}

OpenGL_Hook::OpenGL_Hook():
    hooked(false),
    initialized(false),
    last_window(nullptr),
    wglSwapBuffers(nullptr)
{
    _library = LoadLibrary(DLL_NAME);
}

OpenGL_Hook::~OpenGL_Hook()
{
    APP_LOGI("OpenGL Hook removed\n");

    if (initialized)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
    }

    FreeLibrary(reinterpret_cast<HMODULE>(_library));

    _inst = nullptr;
}

OpenGL_Hook* OpenGL_Hook::Inst()
{
    if (_inst == nullptr)
        _inst = new OpenGL_Hook;

    return _inst;
}

const char* OpenGL_Hook::get_lib_name() const
{
    return DLL_NAME;
}

void OpenGL_Hook::loadFunctions(wglSwapBuffers_t pfnwglSwapBuffers)
{
    wglSwapBuffers = pfnwglSwapBuffers;
}

std::shared_ptr<uint64_t> OpenGL_Hook::CreateImageResource(std::shared_ptr<Image> source)
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