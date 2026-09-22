// Ps4GlesDisplayLists.h — GL_COMPILE display lists on GLES2.
//
// The desktop renderer path Minecraft keeps (RenderGlobal/WorldRenderer terrain
// lists, sky/stars/clouds, model boxes) records geometry once and replays it
// every frame with glCallLists. GLES2 has no display lists, so they are
// emulated here:
//
//   * geometry drawn while a list is open is copied into the list's own staging
//     bytes and, when the list is closed, uploaded once into a static VBO;
//   * every other RenderAPI call made while recording (matrix ops, colour,
//     texture binds, state toggles) is stored as a closure that replays the
//     same RenderAPI call.
//
// That keeps GL_COMPILE semantics -- nothing executes while recording -- and
// replay cost is one VBO bind + draw per recorded batch.
#pragma once
#ifdef PS4_PLATFORM

#include "platform/RenderAPI.h"

#include <functional>

namespace Ps4Gles
{
namespace DisplayLists
{
int generate(int count);
void remove(int first, int count);
void begin(int list);
void end();
bool recording();

// Record a non-geometry call while recording(). Replayed in order.
void record(std::function<void()> command);
// Record geometry while recording(): copies the vertex run.
bool recordDraw(const RenderInterleavedMesh& mesh);

void call(int list);
void callMany(int count, const int* lists);

// GPU bytes held by all compiled lists (for diagnostics).
std::size_t residentBytes();
}
}

#endif // PS4_PLATFORM
