#ifdef PS4_PLATFORM

#include "ps4/input/Ps4Input.h"
#include "ps4/input/Ps4InputMapper.h"
#include "ps4/input/Ps4PadRuntime.h"
#include "ps4/input/Ps4Pointer.h"

namespace Ps4Input
{
void initialize(int screenWidth, int screenHeight)
{
	Ps4Pointer::setBounds(screenWidth, screenHeight);
	Ps4PadRuntime::initialize();
}

void shutdown()
{
	Ps4PadRuntime::shutdown();
}

void poll(bool inMenu, bool specializedMenuNavigation)
{
	Ps4PadRuntime::poll();
	Ps4InputMapper::update(inMenu, specializedMenuNavigation);
}

void setMenuCursor(int x, int y)
{
	Ps4Pointer::setPosition(x, y);
}
}

#endif // PS4_PLATFORM
