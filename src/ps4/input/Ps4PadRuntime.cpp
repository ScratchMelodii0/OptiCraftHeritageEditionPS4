#ifdef PS4_PLATFORM

#include "ps4/input/Ps4PadRuntime.h"
#include "ps4/input/Ps4PadState.h"
#include "ps4/system/Ps4Modules.h"
#include "platform/Log.h"

#include <stdint.h>
#include <orbis/Pad.h>

namespace
{
int s_handle = -1;
bool s_initialized = false;
std::uint32_t s_previousHeld = 0;
int s_reopenCountdown = 0;

// scePadOpen fails while no controller is paired to the user; retrying every
// frame would only spam the log, so back off to about once a second.
constexpr int kReopenIntervalFrames = 60;

float normaliseAxis(uint8_t value)
{
	int centred = static_cast<int>(value) - 128;
	if (centred < -127) centred = -127;
	return static_cast<float>(centred) / 127.0f;
}

void publishDisconnected()
{
	s_previousHeld = 0;
	ps4PadUpdateSnapshot(Ps4PadSnapshot{});
}

void tryOpen()
{
	if (s_handle >= 0)
		return;
	if (s_reopenCountdown > 0)
	{
		--s_reopenCountdown;
		return;
	}
	s_handle = scePadOpen(Ps4Modules::initialUser(), ORBIS_PAD_PORT_TYPE_STANDARD, 0, nullptr);
	if (s_handle < 0)
	{
		s_reopenCountdown = kReopenIntervalFrames;
		return;
	}
	MC_LOG_INFO("input", "[PS4] pad opened (handle %d)\n", s_handle);
}
}

namespace Ps4PadRuntime
{
bool initialize()
{
	if (s_initialized)
		return true;
	const int rc = scePadInit();
	if (rc < 0)
	{
		MC_LOG_ERROR("input", "[PS4] scePadInit failed: 0x%08X\n", static_cast<unsigned>(rc));
		return false;
	}
	s_initialized = true;
	tryOpen();
	return true;
}

void shutdown()
{
	if (s_handle >= 0)
		scePadClose(s_handle);
	s_handle = -1;
	s_initialized = false;
	publishDisconnected();
}

void poll()
{
	if (!s_initialized)
		return;
	tryOpen();
	if (s_handle < 0)
	{
		publishDisconnected();
		return;
	}

	OrbisPadData data{};
	if (scePadReadState(s_handle, &data) < 0 || !data.connected)
	{
		// A turned-off controller keeps its handle valid and reports
		// disconnected until it is switched back on; nothing is held then.
		publishDisconnected();
		return;
	}

	Ps4PadSnapshot snapshot;
	snapshot.connected = true;
	snapshot.leftX = normaliseAxis(data.leftStick.x);
	snapshot.leftY = normaliseAxis(data.leftStick.y);
	snapshot.rightX = normaliseAxis(data.rightStick.x);
	snapshot.rightY = normaliseAxis(data.rightStick.y);
	snapshot.held = data.buttons;
	snapshot.pressed = data.buttons & ~s_previousHeld;
	snapshot.released = s_previousHeld & ~data.buttons;
	s_previousHeld = data.buttons;
	ps4PadUpdateSnapshot(snapshot);
}
}

#endif // PS4_PLATFORM
