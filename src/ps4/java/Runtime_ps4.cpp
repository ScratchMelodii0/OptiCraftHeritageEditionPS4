// Runtime_ps4.cpp — PS4 implementation of java/Runtime.h.
//
// Game-process heap comes out of flexible memory, which the kernel reports
// directly. The ceiling is the flexible memory free when the numbers were first
// asked for (before the world loads) and "free" is what is still available, so
// the debug screen's used/max figures track real consumption. Only the F3
// overlay and memory diagnostics read these; nothing sizes itself from them.
#ifdef PS4_PLATFORM

#include "java/Runtime.h"

#include <stdint.h>
#include <orbis/libkernel.h>

namespace
{
long_t availableFlexibleBytes()
{
	size_t available = 0;
	if (sceKernelAvailableFlexibleMemorySize(&available) < 0)
		return 0;
	return static_cast<long_t>(available);
}

long_t ceilingBytes()
{
	static const long_t ceiling = availableFlexibleBytes();
	return ceiling > 0 ? ceiling : 1;
}
}

Runtime Runtime::instance;

Runtime &Runtime::getRuntime()
{
	return instance;
}

long_t Runtime::maxMemory()
{
	return ceilingBytes();
}

long_t Runtime::totalMemory()
{
	return ceilingBytes();
}

long_t Runtime::freeMemory()
{
	const long_t available = availableFlexibleBytes();
	const long_t ceiling = ceilingBytes();
	// Java freeMemory must never exceed totalMemory.
	return available < ceiling ? available : ceiling;
}

#endif // PS4_PLATFORM
