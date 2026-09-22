#ifdef PS4_PLATFORM

#include "ps4/input/Ps4AnalogFilter.h"

namespace Ps4AnalogFilter
{
float apply(float value, float deadzone)
{
	if (value > -deadzone && value < deadzone)
		return 0.0f;
	const float sign = value < 0.0f ? -1.0f : 1.0f;
	float output = (value * sign - deadzone) / (1.0f - deadzone);
	if (output < 0.0f) output = 0.0f;
	if (output > 1.0f) output = 1.0f;
	return output * sign;
}
}

#endif // PS4_PLATFORM
