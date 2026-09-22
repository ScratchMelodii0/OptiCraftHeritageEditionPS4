// Ps4DebugLog.h — early, dependency-free logging for the PS4 port.
//
// Usable before the shared platform/Log.h sink is up (and by the bring-up
// target, which links none of it). Lines go to the kernel debug channel, which
// GoldHEN/Mira klog and shadPS4's console both show, and optionally to a file
// under /data so a crash on retail hardware still leaves evidence behind.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4DebugLog
{
// Opens <path> for appending (truncated on first open). Safe to skip: without
// a file, output only goes to the kernel debug channel.
bool openFile(const char* path);
void closeFile();

void printf(const char* format, ...) __attribute__((format(printf, 1, 2)));
}

#endif
