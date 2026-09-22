// Ps4Modules.h — system module loading for the PS4 port.
//
// The PS4 loads most of libSce* lazily: the stub libraries resolve at link
// time, but the backing .sprx must be started before its first call or the
// import traps. Everything the port needs is started here, in one place, so a
// missing module fails loudly at boot instead of as a crash mid-game.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4Modules
{
// Starts the system modules plus Piglet (GLES2) from the sandbox common lib
// directory. Returns false, with the reason logged, if a required one fails.
bool loadRequired();

// Optional modules (network). Failure is logged and reported, never fatal.
bool loadNetwork();

// Id of the user who launched the application, or -1 before loadRequired().
int initialUser();
}

#endif
