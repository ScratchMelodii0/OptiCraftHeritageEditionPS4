// Ps4GlesPipeline.h — desktop GL 1.x fixed-function semantics on Piglet GLES2.
//
// The game renders through platform/RenderAPI.h, whose vocabulary is the
// OpenGL 1.x pipeline Minecraft 1.2.5 was written against: matrix stacks,
// glEnable(GL_FOG/GL_LIGHTING/GL_ALPHA_TEST), two texture units with the
// lightmap on the second, client arrays and display lists. GLES2 has none of
// it. This module keeps that state on the CPU and draws everything with one
// "uber" shader whose uniforms reproduce the fixed-function equations:
//
//   * transform: modelview/projection stacks, per-unit texture matrices;
//   * lighting: GL_LIGHT0/1 directional, GL_COLOR_MATERIAL ambient+diffuse,
//     light-model ambient (all Minecraft uses -- no specular, no spot);
//   * texturing: unit 0 MODULATE, unit 1 (lightmap) MODULATE;
//   * fog: LINEAR / EXP / EXP2, planar or NV radial distance;
//   * alpha test: all eight compare functions, as a discard.
//
// Display lists (GL_COMPILE) are recorded here as well; see Ps4GlesDisplayLists.
//
// Everything is single-threaded and assumes the EGL context of Ps4Piglet is
// current on the calling thread, exactly like the desktop GL backend.
#pragma once
#ifdef PS4_PLATFORM

#include "platform/RenderAPI.h"
#include "ps4/render/Ps4GlesMatrix.h"

#include <cstdint>

namespace Ps4Gles
{
// Creates the shader program, the streaming vertex buffer and the static quad
// index buffer. Returns false (with the reason logged) if the shader cannot be
// compiled or loaded; nothing else in this module may be used after that.
bool initialize();
void shutdown();

// --- Fixed-function state ---------------------------------------------------
void setCapability(RenderCapability capability, bool enabled);
bool capabilityEnabled(RenderCapability capability);

void setColor(float r, float g, float b, float a);
void setNormal(float x, float y, float z);
void setMultiTexCoord(int unit, float u, float v);

// unit: 0 or 1 (callers pass GL_TEXTURE0/1 enums; the backend maps them).
void setActiveTextureUnit(int unit);
int activeTextureUnit();
void bindTexture(int texture);

void setAlphaFunc(RenderCompare function, float reference);
void setFogMode(RenderFogMode mode);
void setFogRadial(bool radial);
void setFogParameter(RenderFogParameter parameter, float value);
void setFogColor(const float* rgba);
void setLight(int light, RenderLightParameter parameter, const float* values);
void setLightModelAmbient(const float* rgba);

// --- Matrices ----------------------------------------------------------------
void setMatrixMode(RenderMatrixMode mode);
Mat4& currentMatrix();               // of the current mode (and texture unit)
void matrixChanged();                // call after mutating currentMatrix()
void pushMatrix();
void popMatrix();
void getMatrix(RenderMatrixQuery query, float* out16);

// --- Geometry ----------------------------------------------------------------
// Draws `mesh` with the current state. Client-memory data is streamed into a
// ring VBO; quads are drawn through a shared index buffer.
bool drawInterleaved(const RenderInterleavedMesh& mesh);

// Same, but the vertex data already lives in buffer object `vbo` at `offset`
// (display-list replay). The mesh's `data` field is ignored.
bool drawFromBuffer(unsigned int vbo, std::size_t offset, const RenderInterleavedMesh& mesh);

// Frame boundary bookkeeping for the streaming buffer.
void endFrame();
}

#endif // PS4_PLATFORM
