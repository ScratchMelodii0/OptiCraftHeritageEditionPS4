// ClientPlatformPolicy_PS4.cpp — PS4 client policy.
//
// The game directory (options.txt, saves/, stats, screenshots) is the
// homebrew-writable /data/opticraft rather than a folder beside the
// executable: /app0 is the read-only installed package.
#include "platform/ClientPlatformPolicy.h"

#include "pc/CrashHandler.h"
#include "ps4/system/Ps4Paths.h"
#include "ps4/system/Ps4Piglet.h"

namespace ClientPlatformPolicy
{
int initialWidth()
{
    return Ps4Piglet::width();
}

int initialHeight()
{
    return Ps4Piglet::height();
}

std::string minecraftDirectory()
{
    return Ps4Paths::writableRoot();
}

bool saveConverterUsesSavesSubdirectory()
{
    return true;
}

void applyGameSettingsDefaults(GameSettings*)
{
}

void preloadStartupTextures(RenderEngine*)
{
}

void releaseWorldEntryAssets(RenderEngine*)
{
}

int panoramaSampleGrid()
{
    return 8;
}

void reportCrash(const std::string& description)
{
    CrashHandler::Crash(description);
}
}
