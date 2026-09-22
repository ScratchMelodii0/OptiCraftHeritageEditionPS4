// Ps4Input.h — DualShock 4 input for the PS4 port.
//
// Polls libScePad once per frame and feeds the game the same way the PS2 and
// Wii front ends do: gameplay actions become lwjgl::Keyboard/Mouse events
// through the configurable button bindings, the sticks drive movement and the
// camera, and while a GUI screen is open the right stick moves the software
// cursor and the face buttons click it.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4Input
{
void initialize(int screenWidth, int screenHeight);
void shutdown();

// inMenu: a GuiScreen is open. specializedMenuNavigation: that screen drives
// its own D-pad/button navigation (containers, text fields).
void poll(bool inMenu, bool specializedMenuNavigation);

// Place the software cursor at an absolute framebuffer position (container
// slot navigation and keyboard-selected buttons move it this way).
void setMenuCursor(int x, int y);
}

#endif
