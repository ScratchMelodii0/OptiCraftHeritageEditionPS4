#ifdef PS4_PLATFORM

#include "ps4/audio/Ps4AudioOut.h"
#include "ps4/audio/Ps4AudioMixer.h"
#include "ps4/system/Ps4Modules.h"
#include "platform/Log.h"
#include "platform/Thread.h"

#include <stdint.h>
#include <orbis/AudioOut.h>

#include <atomic>

namespace Ps4Audio
{
namespace
{
constexpr int kBlockFrames = 256;

int s_port = -1;
Mixer* s_mixer = nullptr;
std::atomic<bool> s_running(false);
PlatformThread s_thread;

void* outputThread(void*)
{
	alignas(64) std::int16_t block[kBlockFrames * 2];
	while (s_running.load(std::memory_order_acquire))
	{
		s_mixer->mix(block, kBlockFrames);
		if (sceAudioOutOutput(s_port, block) < 0)
			break;
	}
	// Drain the last block before the port is closed.
	sceAudioOutOutput(s_port, nullptr);
	return nullptr;
}
}

bool startOutput(Mixer& mixer)
{
	if (s_running.load())
		return true;

	// A failure here is most often "already initialised"; whether the library
	// is really unusable is settled by sceAudioOutOpen below.
	const int rc = sceAudioOutInit();
	if (rc < 0)
		MC_LOG_INFO("audio", "[PS4] sceAudioOutInit returned 0x%08X\n", static_cast<unsigned>(rc));
	s_port = sceAudioOutOpen(Ps4Modules::initialUser(), ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
	                         kBlockFrames, kOutputRate, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
	if (s_port < 0)
	{
		MC_LOG_ERROR("audio", "[PS4] sceAudioOutOpen failed: 0x%08X\n", static_cast<unsigned>(s_port));
		s_port = -1;
		return false;
	}

	s_mixer = &mixer;
	s_running.store(true, std::memory_order_release);
	// High priority and a modest stack: the loop only mixes and blocks.
	if (!s_thread.start(outputThread, nullptr, 64 * 1024, 16))
	{
		s_running.store(false);
		sceAudioOutClose(s_port);
		s_port = -1;
		MC_LOG_ERROR("audio", "[PS4] audio thread failed to start\n");
		return false;
	}
	MC_LOG_INFO("audio", "[PS4] audio out ready (port %d)\n", s_port);
	return true;
}

void stopOutput()
{
	if (!s_running.exchange(false))
		return;
	s_thread.join();
	sceAudioOutClose(s_port);
	s_port = -1;
	s_mixer = nullptr;
}
}

#endif // PS4_PLATFORM
