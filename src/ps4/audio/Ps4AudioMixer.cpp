#ifdef PS4_PLATFORM

#include "ps4/audio/Ps4AudioMixer.h"

#include "pc/external/stb_vorbis.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

namespace Ps4Audio
{
namespace
{
// How far ahead of the play position the stream decoder keeps its buffer, in
// source frames: ~85 ms at 48 kHz, a few mixer blocks, so each refill is one
// short stb_vorbis call on the audio thread.
constexpr std::size_t kStreamRefillFrames = 4096;

std::int16_t clampSample(std::int32_t value)
{
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return static_cast<std::int16_t>(value);
}

// Linear interpolation between source frames i and i+1 (clamped at the end).
inline void sampleFrame(const std::int16_t* samples, std::size_t frames, int channels,
                        double position, float& left, float& right)
{
	const std::size_t i = static_cast<std::size_t>(position);
	const std::size_t j = i + 1 < frames ? i + 1 : i;
	const float t = static_cast<float>(position - static_cast<double>(i));
	if (channels == 1)
	{
		const float a = samples[i], b = samples[j];
		left = right = a + (b - a) * t;
	}
	else
	{
		const float la = samples[i * channels], lb = samples[j * channels];
		const float ra = samples[i * channels + 1], rb = samples[j * channels + 1];
		left = la + (lb - la) * t;
		right = ra + (rb - ra) * t;
	}
}
}

Mixer::Mixer() = default;

Mixer::~Mixer()
{
	std::lock_guard<PlatformMutex> lock(mutex_);
	closeStreamLocked();
}

void Mixer::playClip(const std::shared_ptr<const PcmClip>& clip, float volume, float pitch)
{
	if (!clip || clip->frames() == 0 || clip->channels < 1 || clip->channels > 2 || volume <= 0.0f)
		return;
	if (pitch <= 0.0f)
		pitch = 1.0f;

	std::lock_guard<PlatformMutex> lock(mutex_);
	int slot = -1;
	for (int i = 0; i < kMaxSfxVoices; ++i)
	{
		if (!sfx_[i].clip)
		{
			slot = i;
			break;
		}
		if (slot < 0 || sfx_[i].volume < sfx_[slot].volume)
			slot = i;
	}
	if (sfx_[slot].clip && sfx_[slot].volume > volume)
		return;   // every voice is busy with something louder

	SfxVoice& voice = sfx_[slot];
	voice.clip = clip;
	voice.position = 0.0;
	voice.step = static_cast<double>(clip->sampleRate) / kOutputRate * pitch;
	voice.volume = std::min(volume, 1.0f);
}

void Mixer::startStream(stb_vorbis* decoder, StreamKind kind, float volume)
{
	std::lock_guard<PlatformMutex> lock(mutex_);
	closeStreamLocked();
	if (decoder == nullptr)
		return;
	const stb_vorbis_info info = stb_vorbis_get_info(decoder);
	stream_.decoder = decoder;
	stream_.kind = kind;
	stream_.volume = std::min(std::max(volume, 0.0f), 1.0f);
	// stb_vorbis downmixes anything wider than stereo when asked for 2.
	stream_.channels = info.channels >= 2 ? 2 : 1;
	stream_.step = static_cast<double>(info.sample_rate) / kOutputRate;
	stream_.position = 0.0;
	stream_.buffer.clear();
	stream_.finished = false;
}

void Mixer::stopStream()
{
	std::lock_guard<PlatformMutex> lock(mutex_);
	closeStreamLocked();
}

void Mixer::setStreamVolume(float volume)
{
	std::lock_guard<PlatformMutex> lock(mutex_);
	stream_.volume = std::min(std::max(volume, 0.0f), 1.0f);
}

StreamKind Mixer::streamKind() const
{
	std::lock_guard<PlatformMutex> lock(mutex_);
	return stream_.decoder != nullptr ? stream_.kind : StreamKind::None;
}

bool Mixer::streamActive() const
{
	std::lock_guard<PlatformMutex> lock(mutex_);
	return stream_.decoder != nullptr;
}

void Mixer::stopAll()
{
	std::lock_guard<PlatformMutex> lock(mutex_);
	for (SfxVoice& voice : sfx_)
		voice = SfxVoice{};
	closeStreamLocked();
}

void Mixer::closeStreamLocked()
{
	if (stream_.decoder != nullptr)
		stb_vorbis_close(stream_.decoder);
	stream_ = StreamVoice{};
}

void Mixer::mix(std::int16_t* out, int frames)
{
	if (out == nullptr || frames <= 0)
		return;
	accum_.assign(static_cast<std::size_t>(frames) * 2u, 0);
	{
		std::lock_guard<PlatformMutex> lock(mutex_);
		mixSfx(accum_.data(), frames);
		mixStream(accum_.data(), frames);
	}
	for (std::size_t i = 0; i < accum_.size(); ++i)
		out[i] = clampSample(accum_[i]);
}

void Mixer::mixSfx(std::int32_t* accum, int frames)
{
	for (SfxVoice& voice : sfx_)
	{
		if (!voice.clip)
			continue;
		const PcmClip& clip = *voice.clip;
		const std::size_t clipFrames = clip.frames();
		for (int f = 0; f < frames; ++f)
		{
			if (voice.position >= static_cast<double>(clipFrames))
			{
				voice.clip.reset();
				break;
			}
			float left = 0.0f, right = 0.0f;
			sampleFrame(clip.samples.data(), clipFrames, clip.channels, voice.position, left, right);
			accum[f * 2] += static_cast<std::int32_t>(left * voice.volume);
			accum[f * 2 + 1] += static_cast<std::int32_t>(right * voice.volume);
			voice.position += voice.step;
		}
	}
}

bool Mixer::refillStream(std::size_t framesWanted)
{
	StreamVoice& s = stream_;
	const std::size_t channels = static_cast<std::size_t>(s.channels);
	std::size_t buffered = s.buffer.size() / channels;

	// Drop what has been played so the buffer stays a small window.
	const std::size_t consumed = static_cast<std::size_t>(s.position);
	if (consumed > 0)
	{
		const std::size_t drop = std::min(consumed, buffered);
		s.buffer.erase(s.buffer.begin(), s.buffer.begin() + static_cast<std::ptrdiff_t>(drop * channels));
		s.position -= static_cast<double>(drop);
		buffered -= drop;
	}

	while (!s.finished && buffered < framesWanted)
	{
		const std::size_t old = s.buffer.size();
		s.buffer.resize(old + kStreamRefillFrames * channels);
		const int got = stb_vorbis_get_samples_short_interleaved(
			s.decoder, s.channels, s.buffer.data() + old, static_cast<int>(kStreamRefillFrames * channels));
		if (got <= 0)
		{
			s.buffer.resize(old);
			s.finished = true;
			break;
		}
		s.buffer.resize(old + static_cast<std::size_t>(got) * channels);
		buffered += static_cast<std::size_t>(got);
	}
	return buffered > 0;
}

void Mixer::mixStream(std::int32_t* accum, int frames)
{
	StreamVoice& s = stream_;
	if (s.decoder == nullptr)
		return;

	// Source frames this block will touch, plus one for interpolation. The
	// refill first drops played frames, leaving position below one frame.
	const std::size_t needed = static_cast<std::size_t>(std::ceil(s.step * frames)) + 2;
	if (!refillStream(needed))
	{
		closeStreamLocked();
		return;
	}

	const std::size_t buffered = s.buffer.size() / static_cast<std::size_t>(s.channels);
	for (int f = 0; f < frames; ++f)
	{
		if (s.position >= static_cast<double>(buffered))
		{
			if (s.finished)
				closeStreamLocked();
			return;
		}
		float left = 0.0f, right = 0.0f;
		sampleFrame(s.buffer.data(), buffered, s.channels, s.position, left, right);
		accum[f * 2] += static_cast<std::int32_t>(left * s.volume);
		accum[f * 2 + 1] += static_cast<std::int32_t>(right * s.volume);
		s.position += s.step;
	}
}
}

#endif // PS4_PLATFORM
