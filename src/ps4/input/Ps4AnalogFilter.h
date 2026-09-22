// Ps4AnalogFilter.h — radial-free per-axis deadzone with rescaling, the same
// response curve the PS2 port uses (ps2/input/Ps2AnalogFilter), so a stick
// just past the deadzone starts at 0 instead of jumping.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4AnalogFilter
{
// A DS4 stick rests within about +-0.08 of centre, far tighter than a PS2
// pad, so the default deadzone is smaller.
constexpr float kDefaultDeadzone = 0.12f;
float apply(float value, float deadzone = kDefaultDeadzone);
}

#endif
