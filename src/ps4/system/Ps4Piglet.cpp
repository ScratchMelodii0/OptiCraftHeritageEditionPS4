#ifdef PS4_PLATFORM

#include "ps4/system/Ps4Piglet.h"
#include "ps4/system/Ps4DebugLog.h"

#include <orbis/libkernel.h>
#include <orbis/Pigletv2VSH.h>   // pulls in EGL/egl.h and GLES2/gl2.h

#include <cstring>

namespace
{
EGLDisplay s_display = EGL_NO_DISPLAY;
EGLSurface s_surface = EGL_NO_SURFACE;
EGLContext s_context = EGL_NO_CONTEXT;
int s_width = 0;
int s_height = 0;

void logEglFailure(const char* what)
{
    Ps4DebugLog::printf("[ps4] %s failed: EGL 0x%04X\n", what, static_cast<unsigned>(eglGetError()));
}
}

namespace Ps4Piglet
{
bool initialize(int width, int height)
{
    // Memory budget handed to Piglet. The values follow flatz's ps4_gl_test
    // (via the OpenOrbis piglet sample), which is known to work on retail
    // firmware and under shadPS4. The video pool holds every texture and VBO,
    // so it is the one worth being generous with.
    OrbisPglConfig config;
    std::memset(&config, 0, sizeof(config));
    config.size = sizeof(config);
    config.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT | ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
    config.processOrder = 1;
    config.systemSharedMemorySize = 250u * 1024u * 1024u;
    config.videoSharedMemorySize = 512u * 1024u * 1024u;
    config.maxMappedFlexibleMemory = 170u * 1024u * 1024u;
    config.drawCommandBufferSize = 1u * 1024u * 1024u;
    config.lcueResourceBufferSize = 1u * 1024u * 1024u;
    config.dbgPosCmd_0x40 = static_cast<uint32_t>(width);
    config.dbgPosCmd_0x44 = static_cast<uint32_t>(height);
    config.unk_0x5C = 2;
    if (!scePigletSetConfigurationVSH(&config))
    {
        Ps4DebugLog::printf("[ps4] scePigletSetConfigurationVSH failed\n");
        return false;
    }

    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s_display == EGL_NO_DISPLAY) { logEglFailure("eglGetDisplay"); return false; }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(s_display, &major, &minor)) { logEglFailure("eglInitialize"); return false; }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) { logEglFailure("eglBindAPI"); return false; }

    // Unlike the sample, the game needs a depth buffer. Not every firmware or
    // emulator exposes D24S8, so fall back to plain 16-bit depth.
    static const EGLint kDepthStencil[][2] = { { 24, 8 }, { 24, 0 }, { 16, 0 } };
    EGLConfig eglConfig = nullptr;
    EGLint configCount = 0;
    for (const auto& ds : kDepthStencil)
    {
        const EGLint attribs[] = {
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, ds[0], EGL_STENCIL_SIZE, ds[1],
            EGL_SAMPLE_BUFFERS, 0, EGL_SAMPLES, 0,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_NONE,
        };
        if (eglChooseConfig(s_display, attribs, &eglConfig, 1, &configCount) && configCount > 0)
        {
            Ps4DebugLog::printf("[ps4] EGL config: depth %d stencil %d\n", ds[0], ds[1]);
            break;
        }
    }
    if (configCount < 1)
    {
        logEglFailure("eglChooseConfig");
        return false;
    }

    static OrbisPglWindow window;
    window.uID = 0;
    window.uWidth = static_cast<khronos_uint32_t>(width);
    window.uHeight = static_cast<khronos_uint32_t>(height);
    const EGLint windowAttribs[] = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE };
    s_surface = eglCreateWindowSurface(s_display, eglConfig, &window, windowAttribs);
    if (s_surface == EGL_NO_SURFACE) { logEglFailure("eglCreateWindowSurface"); return false; }

    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    s_context = eglCreateContext(s_display, eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (s_context == EGL_NO_CONTEXT) { logEglFailure("eglCreateContext"); return false; }

    if (!eglMakeCurrent(s_display, s_surface, s_surface, s_context)) { logEglFailure("eglMakeCurrent"); return false; }

    s_width = width;
    s_height = height;
    setSwapInterval(1);

    Ps4DebugLog::printf("[ps4] EGL %d.%d  GL_VERSION=%s  GL_RENDERER=%s\n", major, minor,
                        reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                        reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    return true;
}

void shutdown()
{
    if (s_display == EGL_NO_DISPLAY)
        return;
    eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s_context != EGL_NO_CONTEXT) eglDestroyContext(s_display, s_context);
    if (s_surface != EGL_NO_SURFACE) eglDestroySurface(s_display, s_surface);
    eglTerminate(s_display);
    s_context = EGL_NO_CONTEXT;
    s_surface = EGL_NO_SURFACE;
    s_display = EGL_NO_DISPLAY;
}

int width() { return s_width; }
int height() { return s_height; }

void setSwapInterval(int interval)
{
    if (s_display != EGL_NO_DISPLAY && !eglSwapInterval(s_display, interval))
        logEglFailure("eglSwapInterval");
}

void swapBuffers()
{
    if (s_display != EGL_NO_DISPLAY)
        eglSwapBuffers(s_display, s_surface);
}
}

#endif // PS4_PLATFORM
