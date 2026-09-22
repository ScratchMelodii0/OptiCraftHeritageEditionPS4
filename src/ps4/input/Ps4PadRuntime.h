// Ps4PadRuntime.h — libScePad lifecycle for the user who launched the game.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4PadRuntime
{
bool initialize();
void shutdown();
// Reads the pad and publishes a new Ps4PadSnapshot. Reopens the handle after
// a controller is turned off or re-paired.
void poll();
}

#endif
