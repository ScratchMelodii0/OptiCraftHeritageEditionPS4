#ifdef PS4_PLATFORM

#include "ps4/system/Ps4Bootstrap.h"

#include "lwjgl/GLContext.h"
#include "ps4/render/Ps4GlesPipeline.h"
#include "ps4/system/Ps4DebugLog.h"
#include "ps4/system/Ps4Modules.h"
#include "ps4/system/Ps4Paths.h"
#include "ps4/system/Ps4Piglet.h"

#include <orbis/Pad.h>
#include "ps4/render/Ps4Gl.h"

namespace
{
// No console font is available this early, so a fatal boot error is shown as
// a solid colour (red: missing data, magenta: no shaders) until OPTIONS is
// pressed; the reason itself is in the log. Returns false for the caller.
bool showFatalColour(float r, float g, float b)
{
    int pad = -1;
    if (scePadInit() >= 0)
        pad = scePadOpen(Ps4Modules::initialUser(), ORBIS_PAD_PORT_TYPE_STANDARD, 0, nullptr);
    // Without a pad there is nothing to wait for: show it for a few seconds.
    for (int frame = 0; pad >= 0 || frame < 300; ++frame)
    {
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Ps4Piglet::swapBuffers();
        OrbisPadData data{};
        if (pad >= 0 && scePadReadState(pad, &data) >= 0 && (data.buttons & ORBIS_PAD_BUTTON_OPTIONS))
            break;
    }
    if (pad >= 0)
        scePadClose(pad);
    return false;
}
}

namespace Ps4Bootstrap
{
bool initialize()
{
    if (!Ps4Modules::loadRequired())
        return false;

    if (Ps4Paths::ensureWritableRoot())
        Ps4DebugLog::openFile("/data/opticraft/log.txt");
    Ps4DebugLog::printf("[ps4] OptiCraft Heritage starting\n");

#ifdef PS4_ENABLE_NETWORK
    if (!Ps4Modules::loadNetwork())
        Ps4DebugLog::printf("[ps4] network modules unavailable; multiplayer disabled\n");
#endif

    if (!Ps4Piglet::initialize())
        return false;

    if (!Ps4Gles::initialize())
        return showFatalColour(0.6f, 0.0f, 0.6f);

    if (Ps4Paths::gameDataDir() == nullptr)
    {
        Ps4DebugLog::printf("[ps4] game data not found: expected assets/ under /data/opticraft/data "
                            "or /app0/data\n");
        return showFatalColour(0.6f, 0.0f, 0.0f);
    }
    Ps4DebugLog::printf("[ps4] game data: %s\n", Ps4Paths::gameDataDir());

    lwjgl::GLContext::instantiate();
    return true;
}

void shutdown()
{
    Ps4Gles::shutdown();
    Ps4Piglet::shutdown();
    Ps4DebugLog::closeFile();
}
}

#endif // PS4_PLATFORM
