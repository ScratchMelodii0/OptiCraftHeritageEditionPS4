// Ps4Piglet.h — EGL / OpenGL ES 2.0 display bring-up on Piglet.
//
// Piglet is the system's GLES2 implementation (the one the PS4 shell itself
// uses). This file owns only configuration, the EGL surface/context and the
// buffer swap; all rendering state lives in the RenderAPI backend.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4Piglet
{
// Configures Piglet, creates a 1920x1080 window surface and makes a GLES2
// context current on the calling thread. Ps4Modules::loadRequired() first.
bool initialize(int width = 1920, int height = 1080);
void shutdown();

int width();
int height();

// 1 = present on every vblank (60 Hz), 0 = unthrottled.
void setSwapInterval(int interval);
void swapBuffers();
}

#endif
