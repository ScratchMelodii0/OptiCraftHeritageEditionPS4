// Ps4Gl.h — the one include for Piglet EGL/GLES2 in the PS4 port.
//
// orbis/Pigletv2VSH.h uses the fixed-width integer types without including
// <stdint.h>, and it is also what switches the Khronos headers to the Piglet
// platform (__PIGLET__). Including it through here keeps both right.
#pragma once
#ifdef PS4_PLATFORM

#include <stdint.h>
#include <orbis/Pigletv2VSH.h>

#endif
