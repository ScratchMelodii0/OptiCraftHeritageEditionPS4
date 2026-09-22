// Ps4Bootstrap.h — PS4 hardware bring-up before the game starts.
//
// Order matters: modules -> log file -> Piglet/EGL -> GLES pipeline (shader)
// -> data check -> lwjgl::Display. Every step that can fail does so here, at
// the loader, with the reason on the klog and in /data/opticraft/log.txt.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4Bootstrap
{
bool initialize();
void shutdown();
}

#endif
