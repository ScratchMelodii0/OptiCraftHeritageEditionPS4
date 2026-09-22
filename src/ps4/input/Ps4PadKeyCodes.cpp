#ifdef PS4_PLATFORM

#include "ps4/input/Ps4PadKeyCodes.h"

const char *ps4PadKeyName(int key)
{
	switch (key)
	{
		case PS4_KEY_CROSS: return "Cross";
		case PS4_KEY_CIRCLE: return "Circle";
		case PS4_KEY_TRIANGLE: return "Triangle";
		case PS4_KEY_SQUARE: return "Square";
		case PS4_KEY_L1: return "L1";
		case PS4_KEY_R1: return "R1";
		case PS4_KEY_L2: return "L2";
		case PS4_KEY_R2: return "R2";
		case PS4_KEY_L3: return "L3";
		case PS4_KEY_R3: return "R3";
		case PS4_KEY_DPAD_UP: return "D-Pad Up";
		case PS4_KEY_DPAD_DOWN: return "D-Pad Down";
		case PS4_KEY_DPAD_LEFT: return "D-Pad Left";
		case PS4_KEY_DPAD_RIGHT: return "D-Pad Right";
		default: return nullptr;
	}
}

#endif // PS4_PLATFORM
