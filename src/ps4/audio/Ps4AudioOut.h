// Ps4AudioOut.h — the libSceAudioOut port and the thread that feeds it.
//
// One MAIN port, 48 kHz interleaved S16 stereo, 256-frame blocks. The thread
// mixes a block and hands it to sceAudioOutOutput, which blocks until the
// hardware needs the next one, so the output pacing is the hardware's.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4Audio
{
class Mixer;

bool startOutput(Mixer& mixer);
void stopOutput();
}

#endif
