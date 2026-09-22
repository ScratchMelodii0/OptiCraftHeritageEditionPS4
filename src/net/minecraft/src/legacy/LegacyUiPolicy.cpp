#include "LegacyUiPolicy.h"
#include "platform/PlatformConfig.h"

bool legacyUiDefaultEnabled()
{
#if PLATFORM_GAMEPAD_UI
    return true;
#else
    return false;
#endif
}

// hardcoded badd
const char *legacyUiTitleResourcePath()
{
    return "/legacy/title.png";
}

std::string legacyUiOptionLabel(bool enabled)
{
    return std::string("Legacy UI: ") + (enabled ? "ON" : "OFF");
}
