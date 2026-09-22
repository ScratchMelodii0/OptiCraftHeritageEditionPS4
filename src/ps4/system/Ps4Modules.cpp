#ifdef PS4_PLATFORM

#include "ps4/system/Ps4Modules.h"
#include "ps4/system/Ps4DebugLog.h"

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>

#include <cstdio>

namespace
{
int s_initialUser = -1;

bool loadInternal(OrbisSysModuleInternal module, const char* name)
{
    const int rc = static_cast<int>(sceSysmoduleLoadModuleInternal(module));
    if (rc < 0)
    {
        Ps4DebugLog::printf("[ps4] sysmodule %s failed: 0x%08X\n", name, static_cast<unsigned>(rc));
        return false;
    }
    return true;
}

// Piglet is not a sysmodule; it lives in the sandbox's common/lib directory
// and is started by path, exactly as the OpenOrbis piglet sample does.
bool loadSandboxModule(const char* file)
{
    char path[256];
    std::snprintf(path, sizeof(path), "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), file);
    int startResult = 0;
    const int handle = static_cast<int>(sceKernelLoadStartModule(path, 0, nullptr, 0, nullptr, &startResult));
    if (handle < 0)
    {
        Ps4DebugLog::printf("[ps4] module %s failed: 0x%08X\n", path, static_cast<unsigned>(handle));
        return false;
    }
    return true;
}
}

namespace Ps4Modules
{
bool loadRequired()
{
    if (!loadInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE, "SystemService")) return false;
    if (!loadInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE, "UserService")) return false;
    if (!loadInternal(ORBIS_SYSMODULE_INTERNAL_PAD, "Pad")) return false;
    if (!loadInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT, "AudioOut")) return false;
    if (!loadSandboxModule("libScePigletv2VSH.sprx")) return false;
    // Runtime GLSL compiler for Piglet. Optional: without it the renderer
    // falls back to precompiled shader binaries (see Ps4GlesShader.cpp).
    if (!loadSandboxModule("libSceShaccVSH.sprx"))
        Ps4DebugLog::printf("[ps4] runtime shader compiler unavailable; precompiled shaders required\n");

    // Hide the system splash only once the modules are up: before this the
    // user sees the package's own splash, after it our first frame.
    sceSystemServiceHideSplashScreen();

    if (sceUserServiceInitialize(nullptr) < 0)
    {
        Ps4DebugLog::printf("[ps4] sceUserServiceInitialize failed\n");
        return false;
    }
    int32_t user = -1;
    if (sceUserServiceGetInitialUser(&user) < 0)
    {
        Ps4DebugLog::printf("[ps4] sceUserServiceGetInitialUser failed\n");
        return false;
    }
    s_initialUser = user;
    return true;
}

bool loadNetwork()
{
    return loadInternal(ORBIS_SYSMODULE_INTERNAL_NET, "Net") &&
           loadInternal(ORBIS_SYSMODULE_INTERNAL_NETCTL, "NetCtl");
}

int initialUser()
{
    return s_initialUser;
}
}

#endif // PS4_PLATFORM
