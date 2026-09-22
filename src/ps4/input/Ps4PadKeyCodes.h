// Ps4PadKeyCodes.h — synthetic key codes for DualShock 4 buttons.
//
// Same scheme as the PS2 port (see ps2/input/Ps2PadKeyCodes.h): a pad button is
// named by an int just past lwjgl::Keyboard::KEY_MAX, so GameSettings'
// KeyBinding::keyCode, the Controls screen, its rebind flow and options.txt
// persistence all handle controller bindings unchanged.
//
// OPTIONS (pause) and the touchpad click (debug overlay) keep fixed system
// roles and are not listed, so a rebind can never take them away.
#pragma once
#ifdef PS4_PLATFORM

#include "lwjgl/Keyboard.h"

enum Ps4PadKeyCode : int
{
	PS4_KEY_CROSS = lwjgl::Keyboard::KEY_MAX,
	PS4_KEY_CIRCLE,
	PS4_KEY_TRIANGLE,
	PS4_KEY_SQUARE,
	PS4_KEY_L1,
	PS4_KEY_R1,
	PS4_KEY_L2,
	PS4_KEY_R2,
	PS4_KEY_L3,
	PS4_KEY_R3,
	PS4_KEY_DPAD_UP,
	PS4_KEY_DPAD_DOWN,
	PS4_KEY_DPAD_LEFT,
	PS4_KEY_DPAD_RIGHT,
	PS4_KEY_SENTINEL_END
};

static_assert(PS4_KEY_SENTINEL_END < 256, "Ps4PadKeyCode must fit the 256-slot key-state arrays");

// Display name for the Controls screen, or nullptr if `key` isn't one of these.
const char *ps4PadKeyName(int key);

#endif // PS4_PLATFORM
