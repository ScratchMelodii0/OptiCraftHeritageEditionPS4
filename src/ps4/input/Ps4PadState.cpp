#ifdef PS4_PLATFORM

#include "ps4/input/Ps4PadState.h"

namespace
{
Ps4PadSnapshot s_snapshot;
std::uint32_t s_latchedPressed = 0;
}

const Ps4PadSnapshot &ps4PadGetSnapshot()
{
	return s_snapshot;
}

void ps4PadUpdateSnapshot(const Ps4PadSnapshot &snapshot)
{
	s_snapshot = snapshot;
	s_latchedPressed |= snapshot.pressed;
}

std::uint32_t ps4PadConsumePressed()
{
	const std::uint32_t pressed = s_latchedPressed;
	s_latchedPressed = 0;
	return pressed;
}

void ps4PadLatchPressed(std::uint32_t pressed)
{
	s_latchedPressed |= pressed;
}

void ps4PadClearLatchedPressed()
{
	s_latchedPressed = 0;
}

#endif // PS4_PLATFORM
