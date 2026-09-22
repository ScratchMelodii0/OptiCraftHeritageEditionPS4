// Ps4InputMapper.h — turns the latest Ps4PadSnapshot into lwjgl events.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4InputMapper
{
void update(bool inMenu, bool specializedMenuNavigation);
}

#endif
