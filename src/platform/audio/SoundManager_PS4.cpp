// SoundManager_PS4.cpp — SoundManager on libSceAudioOut via Ps4Audio::Mixer.
//
// Game-side behaviour follows the Wii backend (SoundManager_WII.cpp): music
// and jukebox records share one streaming voice, effects are one-shot voices
// attenuated by distance from the listener. What the PS4 changes is budget:
// with gigabytes instead of Wii MEM2, decoded effects stay cached (up to
// kSfxCacheBudgetBytes) and pitch is honoured by the mixer's resampler.
#ifdef PS4_PLATFORM

#include "net/minecraft/src/SoundManager.h"

#if defined(NO_SOUND)

// PS4_ENABLE_SOUND=OFF: keep the sound pools (other code reads them) but never
// open an audio port. src/ps4/audio is left out of the target in this mode.
void *SoundManager::sndSystem = nullptr;
bool SoundManager::loaded = false;

SoundManager::SoundManager()
	: soundPoolSounds(), soundPoolStreaming(), soundPoolMusic(), soundVolume(0),
	  options(nullptr), rand(), ticksBeforeMusic(0)
{
}

void SoundManager::loadSoundSettings(GameSettings *gamesettings) { options = gamesettings; loaded = false; }
void SoundManager::onSoundOptionsChanged() {}
void SoundManager::closeMinecraft() { loaded = false; }
void SoundManager::addSound(const jstring &s, const std::string &file) { soundPoolSounds.addSound(s, file); }
void SoundManager::addStreaming(const jstring &s, const std::string &file) { soundPoolStreaming.addSound(s, file); }
void SoundManager::addMusic(const jstring &s, const std::string &file) { soundPoolMusic.addSound(s, file); }
void SoundManager::playRandomMusicIfReady() {}
bool SoundManager::playMusicFileNow(const std::string &) { return false; }
void SoundManager::setListenerPosition(EntityLiving *, float) {}
void SoundManager::playStreaming(const jstring &, float, float, float, float, float) {}
void SoundManager::playSound(const jstring &, float, float, float, float, float) {}
void SoundManager::playSoundFX(const jstring &, float, float) {}
void SoundManager::tryToSetLibraryAndCodecs() {}

#else

#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/SoundPoolEntry.h"
#include "platform/Log.h"
#include "platform/audio/AudioAssetFormat.h"
#include "platform/audio/AudioSpatialization.h"
#include "platform/audio/VorbisAssetOpen.h"
#include "ps4/audio/Ps4AudioMixer.h"
#include "ps4/audio/Ps4AudioOut.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
constexpr std::size_t kSfxCacheBudgetBytes = 96u * 1024u * 1024u;

// playSoundFX is the GUI click path; vanilla plays it at a quarter volume.
constexpr float kUiSoundScale = 0.25f;
// Jukebox records are half volume in vanilla, before distance attenuation.
constexpr float kRecordScale = 0.5f;

Ps4Audio::Mixer& mixer()
{
	static Ps4Audio::Mixer instance;
	return instance;
}

AudioListenerState s_listener;
std::unordered_map<std::string, std::shared_ptr<const Ps4Audio::PcmClip>> s_sfxCache;
std::unordered_set<std::string> s_rejectedSfx;
std::size_t s_sfxCacheBytes = 0;

std::shared_ptr<const Ps4Audio::PcmClip> loadClip(const std::string& path)
{
	auto cached = s_sfxCache.find(path);
	if (cached != s_sfxCache.end())
		return cached->second;
	if (s_rejectedSfx.count(path) != 0)
		return nullptr;

	int channels = 0;
	int sampleRate = 0;
	short* decoded = nullptr;
	const int frames = platformDecodeVorbis(path, &channels, &sampleRate, &decoded);
	if (frames <= 0 || decoded == nullptr || channels < 1 || channels > 2)
	{
		std::free(decoded);
		s_rejectedSfx.insert(path);
		MC_LOG_INFO("audio", "[PS4] cannot decode '%s'\n", path.c_str());
		return nullptr;
	}

	auto clip = std::make_shared<Ps4Audio::PcmClip>();
	clip->channels = channels;
	clip->sampleRate = sampleRate;
	clip->samples.assign(decoded, decoded + static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels));
	std::free(decoded);

	const std::size_t bytes = clip->samples.size() * sizeof(std::int16_t);
	// A blunt budget: dropping the whole cache is rare (the effect set is far
	// below the budget) and voices keep their clip alive through shared_ptr.
	if (s_sfxCacheBytes + bytes > kSfxCacheBudgetBytes)
	{
		s_sfxCache.clear();
		s_sfxCacheBytes = 0;
	}
	s_sfxCache.emplace(path, clip);
	s_sfxCacheBytes += bytes;
	return clip;
}

bool startStream(const std::string& path, float volume, Ps4Audio::StreamKind kind)
{
	int error = 0;
	stb_vorbis* decoder = platformOpenVorbis(path, &error);
	if (decoder == nullptr)
	{
		MC_LOG_INFO("audio", "[PS4] cannot open stream '%s' (stb_vorbis %d)\n", path.c_str(), error);
		return false;
	}
	mixer().startStream(decoder, kind, volume);
	return true;
}
}

void *SoundManager::sndSystem = nullptr;
bool SoundManager::loaded = false;

SoundManager::SoundManager()
	: soundPoolSounds(), soundPoolStreaming(), soundPoolMusic(), soundVolume(0),
	  options(nullptr), rand(), ticksBeforeMusic(rand.nextInt(12000))
{
}

void SoundManager::tryToSetLibraryAndCodecs()
{
	if (loaded)
		return;
	loaded = Ps4Audio::startOutput(mixer());
	if (!loaded)
		MC_LOG_ERROR("audio", "[PS4] audio output unavailable; the game runs silent\n");
}

void SoundManager::loadSoundSettings(GameSettings *gamesettings)
{
	soundPoolStreaming.motionX = false;
	options = gamesettings;
	if (!loaded && (gamesettings == nullptr || gamesettings->soundVolume != 0.0f || gamesettings->musicVolume != 0.0f))
		tryToSetLibraryAndCodecs();
}

void SoundManager::onSoundOptionsChanged()
{
	if (!loaded && options && (options->soundVolume != 0.0f || options->musicVolume != 0.0f))
		tryToSetLibraryAndCodecs();
	if (!loaded || options == nullptr)
		return;

	if (mixer().streamKind() == Ps4Audio::StreamKind::Music)
	{
		if (options->musicVolume <= 0.0f)
			mixer().stopStream();
		else
			mixer().setStreamVolume(options->musicVolume);
	}
}

void SoundManager::closeMinecraft()
{
	if (loaded)
	{
		mixer().stopAll();
		Ps4Audio::stopOutput();
		s_sfxCache.clear();
		s_sfxCacheBytes = 0;
	}
	loaded = false;
}

void SoundManager::addSound(const jstring &s, const std::string &file)
{
	if (audioPathHasExtension(file, ".ogg"))
		soundPoolSounds.addSound(s, file);
}

void SoundManager::addStreaming(const jstring &s, const std::string &file)
{
	if (audioPathHasExtension(file, ".ogg"))
		soundPoolStreaming.addSound(s, file);
}

void SoundManager::addMusic(const jstring &s, const std::string &file)
{
	if (audioPathHasExtension(file, ".ogg"))
		soundPoolMusic.addSound(s, file);
}

bool SoundManager::playMusicFileNow(const std::string &file)
{
	if (!loaded || !options || options->musicVolume == 0.0f)
		return false;
	if (!audioPathHasExtension(file, ".ogg") || mixer().streamActive())
		return false;
	if (!startStream(file, options->musicVolume, Ps4Audio::StreamKind::Music))
		return false;
	ticksBeforeMusic = rand.nextInt(12000) + 12000;
	return true;
}

void SoundManager::playRandomMusicIfReady()
{
	if (!loaded || !options || options->musicVolume == 0.0f)
		return;
	// Music and records share the streaming voice; never cut a record off.
	if (mixer().streamActive())
		return;
	if (ticksBeforeMusic > 0)
	{
		--ticksBeforeMusic;
		return;
	}

	SoundPoolEntry *entry = soundPoolMusic.getRandomSound();
	ticksBeforeMusic = rand.nextInt(12000) + 12000;
	if (entry != nullptr)
		startStream(entry->soundUrl, options->musicVolume, Ps4Audio::StreamKind::Music);
}

void SoundManager::setListenerPosition(EntityLiving *entityliving, float partialTick)
{
	if (!loaded || !options || options->soundVolume == 0.0f)
		return;
	updateAudioListener(s_listener, entityliving, partialTick);
}

void SoundManager::playStreaming(const jstring &s, float x, float y, float z, float f3, float)
{
	if (!loaded || !options || (options->soundVolume == 0.0f && !s.empty()))
		return;

	// An empty name is the jukebox being emptied: stop its record only.
	if (s.empty())
	{
		if (mixer().streamKind() == Ps4Audio::StreamKind::Record)
			mixer().stopStream();
		return;
	}
	if (f3 <= 0.0f)
		return;

	const float attenuation = audioStreamingAttenuation(s_listener, x, y, z);
	if (attenuation <= 0.0f)
		return;

	SoundPoolEntry *entry = soundPoolStreaming.getRandomSoundFromSoundPool(s);
	if (entry != nullptr &&
	    startStream(entry->soundUrl, kRecordScale * attenuation * options->soundVolume, Ps4Audio::StreamKind::Record))
		ticksBeforeMusic = rand.nextInt(12000) + 12000;
}

void SoundManager::playSound(const jstring &s, float x, float y, float z, float volume, float pitch)
{
	if (!loaded || !options || options->soundVolume == 0.0f || volume <= 0.0f)
		return;

	SoundPoolEntry *entry = soundPoolSounds.getRandomSoundFromSoundPool(s);
	if (entry == nullptr)
		return;
	const float attenuation = audioSpatialAttenuation(s_listener, x, y, z, volume);
	if (attenuation <= 0.0f)
		return;

	mixer().playClip(loadClip(entry->soundUrl),
	                 std::min(volume, 1.0f) * attenuation * options->soundVolume, pitch);
}

void SoundManager::playSoundFX(const jstring &s, float volume, float pitch)
{
	if (!loaded || !options || options->soundVolume == 0.0f)
		return;

	SoundPoolEntry *entry = soundPoolSounds.getRandomSoundFromSoundPool(s);
	if (entry != nullptr)
		mixer().playClip(loadClip(entry->soundUrl),
		                 std::min(volume, 1.0f) * kUiSoundScale * options->soundVolume, pitch);
}

#endif // NO_SOUND
#endif // PS4_PLATFORM
