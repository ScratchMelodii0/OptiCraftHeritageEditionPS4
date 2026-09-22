// Ps4Bringup.cpp — OpenOrbis toolchain / hardware smoke test (PS4_BRINGUP=ON).
//
// Proves, without any game code, that: the ELF links against the OpenOrbis
// stubs, create-fself produces a bootable eboot.bin, the system modules and
// Piglet start, an EGL/GLES2 context presents at 60 Hz, and the DualShock 4
// can be read. The screen cycles colour; face buttons tint it; OPTIONS exits.
#ifdef PS4_PLATFORM

#include "ps4/system/Ps4DebugLog.h"
#include "ps4/system/Ps4Modules.h"
#include "ps4/system/Ps4Piglet.h"

#include <orbis/Pad.h>
#include <orbis/Pigletv2VSH.h>

#include <cmath>
#include <sys/stat.h>

int main()
{
    // /data is the homebrew-writable partition; the game keeps everything
    // under /data/opticraft, so the bring-up log goes there too.
    mkdir("/data/opticraft", 0777);
    Ps4DebugLog::openFile("/data/opticraft/bringup.log");
    Ps4DebugLog::printf("[bringup] OptiCraft PS4 bring-up starting\n");

    if (!Ps4Modules::loadRequired())
        return 1;
    if (!Ps4Piglet::initialize())
        return 1;

    int pad = -1;
    if (scePadInit() >= 0)
        pad = scePadOpen(Ps4Modules::initialUser(), ORBIS_PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Ps4DebugLog::printf("[bringup] pad handle %d\n", pad);

    float phase = 0.0f;
    for (unsigned frame = 0;; ++frame)
    {
        OrbisPadData data{};
        if (pad >= 0 && scePadReadState(pad, &data) >= 0 && data.connected)
        {
            if (data.buttons & ORBIS_PAD_BUTTON_OPTIONS)
                break;
        }

        float r = 0.5f + 0.5f * std::sin(phase);
        float g = 0.5f + 0.5f * std::sin(phase + 2.094f);
        float b = 0.5f + 0.5f * std::sin(phase + 4.188f);
        if (data.buttons & ORBIS_PAD_BUTTON_CROSS)    { r = 0.1f; g = 0.2f; b = 1.0f; }
        if (data.buttons & ORBIS_PAD_BUTTON_CIRCLE)   { r = 1.0f; g = 0.1f; b = 0.1f; }
        if (data.buttons & ORBIS_PAD_BUTTON_TRIANGLE) { r = 0.1f; g = 1.0f; b = 0.3f; }
        if (data.buttons & ORBIS_PAD_BUTTON_SQUARE)   { r = 1.0f; g = 0.2f; b = 1.0f; }
        phase += 0.02f;

        glViewport(0, 0, Ps4Piglet::width(), Ps4Piglet::height());
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Ps4Piglet::swapBuffers();

        if ((frame % 600u) == 0u)
            Ps4DebugLog::printf("[bringup] frame %u sticks L(%u,%u) R(%u,%u)\n", frame,
                                data.leftStick.x, data.leftStick.y, data.rightStick.x, data.rightStick.y);
    }

    Ps4DebugLog::printf("[bringup] exiting\n");
    if (pad >= 0)
        scePadClose(pad);
    Ps4Piglet::shutdown();
    Ps4DebugLog::closeFile();
    return 0;
}

#endif // PS4_PLATFORM
