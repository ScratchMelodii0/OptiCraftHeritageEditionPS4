// Ps4Pointer.h — the software menu cursor driven by the left stick / D-pad.
// Coordinates are framebuffer pixels, top-left origin.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4Pointer
{
void setBounds(int width, int height);
void enterMenu();
void leaveMenu();
// Seconds since the previous menu frame, clamped to 0.1 s.
float beginMenuFrame();
void move(float dx, float dy);
// Sends a Mouse motion event if the integer position changed.
void publish();
void setPosition(int x, int y);
int x();
int y();
}

#endif
