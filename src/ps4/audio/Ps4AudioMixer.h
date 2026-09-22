// Ps4AudioMixer.h — software mixer behind SoundManager_PS4.
//
// libSceAudioOut takes one interleaved 16-bit stereo stream per port at 48 kHz,
// so every sound is mixed here: up to kMaxSfxVoices one-shot effects from
// decoded PCM, plus one streaming voice (music, or a jukebox record) decoded
// incrementally from an Ogg Vorbis file. Voices of any sample rate are
// resampled linearly; the same step applies the pitch.
//
// Everything here is plain C++: Ps4AudioOut owns the console port and the
// thread that calls mix(). The mixer is safe to drive from the game thread
// while that thread mixes.
#pragma once
#ifdef PS4_PLATFORM

#include "platform/Mutex.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct stb_vorbis;

namespace Ps4Audio
{
constexpr int kOutputRate = 48000;
constexpr int kMaxSfxVoices = 32;

// Decoded 16-bit PCM, shared between the cache and any voices playing it.
struct PcmClip
{
	std::vector<std::int16_t> samples;   // interleaved
	int channels = 1;
	int sampleRate = kOutputRate;
	std::size_t frames() const { return channels > 0 ? samples.size() / static_cast<std::size_t>(channels) : 0; }
};

enum class StreamKind
{
	None,
	Music,
	Record,
};

class Mixer
{
public:
	Mixer();
	~Mixer();
	Mixer(const Mixer&) = delete;
	Mixer& operator=(const Mixer&) = delete;

	// Starts a one-shot effect. When every voice is busy the quietest one is
	// replaced, so a new close sound wins over a distant one.
	void playClip(const std::shared_ptr<const PcmClip>& clip, float volume, float pitch);

	// Takes ownership of `decoder` (closed with stb_vorbis_close). Replaces any
	// current stream.
	void startStream(stb_vorbis* decoder, StreamKind kind, float volume);
	void stopStream();
	void setStreamVolume(float volume);
	StreamKind streamKind() const;
	bool streamActive() const;

	void stopAll();

	// Mixes `frames` stereo frames into `out` (interleaved L/R, 16-bit).
	void mix(std::int16_t* out, int frames);

private:
	struct SfxVoice
	{
		std::shared_ptr<const PcmClip> clip;
		double position = 0.0;   // in source frames
		double step = 1.0;       // source frames per output frame
		float volume = 0.0f;
	};

	struct StreamVoice
	{
		stb_vorbis* decoder = nullptr;
		StreamKind kind = StreamKind::None;
		float volume = 0.0f;
		int channels = 0;
		double step = 1.0;
		double position = 0.0;         // into `buffer`, in source frames
		std::vector<std::int16_t> buffer;
		bool finished = false;
	};

	void mixSfx(std::int32_t* accum, int frames);
	void mixStream(std::int32_t* accum, int frames);
	bool refillStream(std::size_t framesWanted);
	void closeStreamLocked();

	mutable PlatformMutex mutex_;
	SfxVoice sfx_[kMaxSfxVoices];
	StreamVoice stream_;
	std::vector<std::int32_t> accum_;
};
}

#endif // PS4_PLATFORM
