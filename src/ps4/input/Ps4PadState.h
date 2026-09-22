// Ps4PadState.h — the most recent DualShock 4 reading, as the rest of the
// port consumes it: normalised sticks and held/pressed/released button masks.
//
// Button bits are libScePad's (ORBIS_PAD_BUTTON_*), which keep the PS2 pad's
// layout for every button the two share; OPTIONS sits where START was and the
// touchpad click is reported as its own high bit.
#pragma once
#ifdef PS4_PLATFORM

#include <cstdint>

enum Ps4PadButton : std::uint32_t
{
	PS4_PAD_L3       = 0x0002,
	PS4_PAD_R3       = 0x0004,
	PS4_PAD_OPTIONS  = 0x0008,
	PS4_PAD_UP       = 0x0010,
	PS4_PAD_RIGHT    = 0x0020,
	PS4_PAD_DOWN     = 0x0040,
	PS4_PAD_LEFT     = 0x0080,
	PS4_PAD_L2       = 0x0100,
	PS4_PAD_R2       = 0x0200,
	PS4_PAD_L1       = 0x0400,
	PS4_PAD_R1       = 0x0800,
	PS4_PAD_TRIANGLE = 0x1000,
	PS4_PAD_CIRCLE   = 0x2000,
	PS4_PAD_CROSS    = 0x4000,
	PS4_PAD_SQUARE   = 0x8000,
	PS4_PAD_TOUCHPAD = 0x100000,
};

struct Ps4PadSnapshot
{
	bool connected = false;
	float leftX = 0.0f;    // -1 (left) .. +1 (right)
	float leftY = 0.0f;    // -1 (up)   .. +1 (down)
	float rightX = 0.0f;
	float rightY = 0.0f;
	std::uint32_t held = 0;
	std::uint32_t pressed = 0;
	std::uint32_t released = 0;
};

const Ps4PadSnapshot &ps4PadGetSnapshot();
void ps4PadUpdateSnapshot(const Ps4PadSnapshot &snapshot);

// Menu presses are latched until a GUI consumer reads them, so a press that
// lands on a frame where no screen polls the pad is not lost.
std::uint32_t ps4PadConsumePressed();
void ps4PadLatchPressed(std::uint32_t pressed);
void ps4PadClearLatchedPressed();

#endif // PS4_PLATFORM
