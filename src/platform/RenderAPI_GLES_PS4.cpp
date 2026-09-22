// RenderAPI_GLES_PS4.cpp — RenderAPI on PlayStation 4 Piglet (OpenGL ES 2.0).
//
// The fixed-function emulation lives in src/ps4/render/Ps4GlesPipeline; this
// file maps the RenderAPI vocabulary onto it and implements GL_COMPILE for the
// display-list path (Ps4GlesDisplayLists). While a list is open, every call
// that a desktop GL list would capture is recorded as a closure that replays
// this same function later; see PS4_RECORD below. Calls GL executes
// immediately even inside a list (object creation, queries, texture uploads)
// run immediately here too.
#include "platform/RenderAPI.h"

#include "ps4/render/Ps4GlesDisplayLists.h"
#include "ps4/render/Ps4GlesPipeline.h"

#include "ps4/render/Ps4Gl.h"

#include <cstring>
#include <string>

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

using namespace Ps4Gles;

#define PS4_RECORD(...)                                         \
    do                                                          \
    {                                                           \
        if (DisplayLists::recording())                          \
        {                                                       \
            DisplayLists::record([=]() { __VA_ARGS__; });       \
            return;                                             \
        }                                                       \
    } while (0)

namespace
{
bool hasExtension(const char* name)
{
    static std::string extensions;
    static bool loaded = false;
    if (!loaded)
    {
        const char* value = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        extensions = value != nullptr ? value : "";
        loaded = value != nullptr;
    }
    const std::size_t length = std::strlen(name);
    std::size_t at = 0;
    while ((at = extensions.find(name, at)) != std::string::npos)
    {
        const bool startOk = at == 0 || extensions[at - 1] == ' ';
        const bool endOk = at + length == extensions.size() || extensions[at + length] == ' ';
        if (startOk && endOk)
            return true;
        at += length;
    }
    return false;
}

// Callers pass desktop enums (GL_TEXTURE0_ARB + n); accept plain indices too.
int textureUnitIndex(int textureUnit)
{
    return textureUnit >= 0x84C0 ? textureUnit - 0x84C0 : textureUnit;
}

GLenum compareToGl(RenderCompare compare)
{
    switch (compare)
    {
        case RenderCompare::Never: return GL_NEVER;
        case RenderCompare::Less: return GL_LESS;
        case RenderCompare::Equal: return GL_EQUAL;
        case RenderCompare::LessEqual: return GL_LEQUAL;
        case RenderCompare::Greater: return GL_GREATER;
        case RenderCompare::NotEqual: return GL_NOTEQUAL;
        case RenderCompare::GreaterEqual: return GL_GEQUAL;
        case RenderCompare::Always: return GL_ALWAYS;
    }
    return GL_ALWAYS;
}

GLenum blendToGl(RenderBlendFactor factor)
{
    switch (factor)
    {
        case RenderBlendFactor::Zero: return GL_ZERO;
        case RenderBlendFactor::One: return GL_ONE;
        case RenderBlendFactor::SrcColor: return GL_SRC_COLOR;
        case RenderBlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case RenderBlendFactor::SrcAlpha: return GL_SRC_ALPHA;
        case RenderBlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case RenderBlendFactor::DstAlpha: return GL_DST_ALPHA;
        case RenderBlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        case RenderBlendFactor::DstColor: return GL_DST_COLOR;
        case RenderBlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
    }
    return GL_ONE;
}

int s_viewport[4] = { 0, 0, 1920, 1080 };
int s_nextQuery = 1;
}

// --- State -------------------------------------------------------------------

void renderEnable(RenderCapability capability)
{
    PS4_RECORD(renderEnable(capability));
    setCapability(capability, true);
}

void renderDisable(RenderCapability capability)
{
    PS4_RECORD(renderDisable(capability));
    setCapability(capability, false);
}

void renderBlendFunc(RenderBlendFactor source, RenderBlendFactor destination)
{
    PS4_RECORD(renderBlendFunc(source, destination));
    glBlendFunc(blendToGl(source), blendToGl(destination));
}

void renderDepthMask(bool enabled)
{
    PS4_RECORD(renderDepthMask(enabled));
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void renderDepthFunc(RenderCompare function)
{
    PS4_RECORD(renderDepthFunc(function));
    glDepthFunc(compareToGl(function));
}

void renderAlphaFunc(RenderCompare function, float reference)
{
    PS4_RECORD(renderAlphaFunc(function, reference));
    setAlphaFunc(function, reference);
}

void renderCullFace(RenderFace face)
{
    PS4_RECORD(renderCullFace(face));
    glCullFace(face == RenderFace::Front ? GL_FRONT : face == RenderFace::Back ? GL_BACK : GL_FRONT_AND_BACK);
}

void renderColorMask(bool red, bool green, bool blue, bool alpha)
{
    PS4_RECORD(renderColorMask(red, green, blue, alpha));
    glColorMask(red ? GL_TRUE : GL_FALSE, green ? GL_TRUE : GL_FALSE,
                blue ? GL_TRUE : GL_FALSE, alpha ? GL_TRUE : GL_FALSE);
}

void renderBindTexture(int texture)
{
    PS4_RECORD(renderBindTexture(texture));
    bindTexture(texture);
}

void renderSetActiveTextureUnit(int textureUnit)
{
    PS4_RECORD(renderSetActiveTextureUnit(textureUnit));
    setActiveTextureUnit(textureUnitIndex(textureUnit));
}

// Client-array state has no separate "client active" unit in the emulation:
// the interleaved mesh already says which arrays carry which unit's data.
void renderSetClientActiveTextureUnit(int)
{
}

void renderSetMultiTextureCoord(int textureUnit, float u, float v)
{
    PS4_RECORD(renderSetMultiTextureCoord(textureUnit, u, v));
    setMultiTexCoord(textureUnitIndex(textureUnit), u, v);
}

void renderSetLightmapColors(const std::uint32_t*, int)
{
    // The lightmap is a real texture on unit 1 here, as on desktop GL.
}

void renderColor4f(float r, float g, float b, float a)
{
    PS4_RECORD(renderColor4f(r, g, b, a));
    setColor(r, g, b, a);
}

void renderColor3f(float r, float g, float b)
{
    PS4_RECORD(renderColor3f(r, g, b));
    setColor(r, g, b, 1.0f);
}

void renderNormal3f(float x, float y, float z)
{
    PS4_RECORD(renderNormal3f(x, y, z));
    setNormal(x, y, z);
}

// --- Textures ------------------------------------------------------------------

void renderGenerateTextures(int count, int* textures)
{
    if (count <= 0 || textures == nullptr)
        return;
    glGenTextures(static_cast<GLsizei>(count), reinterpret_cast<GLuint*>(textures));
}

void renderDeleteTextures(int count, const int* textures)
{
    if (count <= 0 || textures == nullptr)
        return;
    glDeleteTextures(static_cast<GLsizei>(count), reinterpret_cast<const GLuint*>(textures));
}

void renderTextureSubImageRgba(int level, int x, int y, int width, int height, const void* pixels)
{
    glTexSubImage2D(GL_TEXTURE_2D, level, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void renderTextureImageRgba(int level, int width, int height, const void* pixels)
{
    glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

// GLES2 has no GL_TEXTURE_MAX_LEVEL, so a partially uploaded mip chain would
// leave the texture incomplete (it samples black). Mipmaps are therefore
// reported unsupported (see renderSupportsFeature) and the minification
// filters below never select a mip level.
void renderTextureParameters(bool blur, bool, bool clamp)
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, blur ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, blur ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
}

#if PLATFORM_TEXTURE_QUALITY_CONTROLS
void renderApplyTextureQuality(bool blur, int, bool, int anisotropy)
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, blur ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, blur ? GL_LINEAR : GL_NEAREST);
    if (hasExtension("GL_EXT_texture_filter_anisotropic"))
    {
        const int maximum = renderGetMaxAnisotropy();
        const int requested = anisotropy < 1 ? 1 : anisotropy;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        static_cast<float>(requested < maximum ? requested : maximum));
    }
}
#endif

int renderGetMaxAnisotropy()
{
    if (!hasExtension("GL_EXT_texture_filter_anisotropic"))
        return 1;
    GLfloat maximum = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum);
    return maximum > 1.0f ? static_cast<int>(maximum) : 1;
}

int renderGetMaxSamples()
{
    return 0;
}

bool renderTextureBeginUpload(int, int, int, int, bool, bool, bool, bool) { return true; }
bool renderTextureIsValid(int texture) { return texture > 0; }
void renderResetResources() {}

// --- Fog and lighting ------------------------------------------------------------

void renderFogf(RenderFogParameter parameter, float value)
{
    PS4_RECORD(renderFogf(parameter, value));
    setFogParameter(parameter, value);
}

void renderFogi(RenderFogParameter parameter, RenderFogMode value)
{
    PS4_RECORD(renderFogi(parameter, value));
    if (parameter == RenderFogParameter::DistanceMode)
        setFogRadial(value == RenderFogMode::EyeRadial);
    else
        setFogMode(value);
}

void renderFogColor(const float* values)
{
    if (values == nullptr)
        return;
    if (DisplayLists::recording())
    {
        const float c0 = values[0], c1 = values[1], c2 = values[2], c3 = values[3];
        DisplayLists::record([=]() { const float c[4] = { c0, c1, c2, c3 }; renderFogColor(c); });
        return;
    }
    setFogColor(values);
}

void renderLightfv(int lightIndex, RenderLightParameter parameter, const float* values)
{
    if (values == nullptr)
        return;
    if (DisplayLists::recording())
    {
        const float v0 = values[0], v1 = values[1], v2 = values[2], v3 = values[3];
        DisplayLists::record([=]() { const float v[4] = { v0, v1, v2, v3 }; renderLightfv(lightIndex, parameter, v); });
        return;
    }
    setLight(lightIndex, parameter, values);
}

void renderLightModelAmbient(const float* values)
{
    if (values == nullptr)
        return;
    if (DisplayLists::recording())
    {
        const float v0 = values[0], v1 = values[1], v2 = values[2], v3 = values[3];
        DisplayLists::record([=]() { const float v[4] = { v0, v1, v2, v3 }; renderLightModelAmbient(v); });
        return;
    }
    setLightModelAmbient(values);
}

// Minecraft only ever uses GL_AMBIENT_AND_DIFFUSE on the front faces, which is
// what the shader's colour-material path implements.
void renderColorMaterial(RenderFace, RenderColorMaterialMode) {}

// Smooth shading is the only mode GLES2 has; flat is only a desktop speed hint.
void renderShadeModel(RenderShadeModel) {}

// --- Framebuffer -------------------------------------------------------------------

void renderClear(unsigned int mask)
{
    PS4_RECORD(renderClear(mask));
    glClear(static_cast<GLbitfield>(mask));
}

void renderFinishGpu()
{
    glFinish();
}

// eglSwapBuffers is the only synchronisation point on Piglet.
void renderSubmitFrame()
{
}

void renderClearColor(float r, float g, float b, float a)
{
    PS4_RECORD(renderClearColor(r, g, b, a));
    glClearColor(r, g, b, a);
}

void renderClearDepth(double depth)
{
    PS4_RECORD(renderClearDepth(depth));
    glClearDepthf(static_cast<GLfloat>(depth));
}

void renderPolygonOffset(float factor, float units)
{
    PS4_RECORD(renderPolygonOffset(factor, units));
    glPolygonOffset(factor, units);
}

void renderLineWidth(float width)
{
    PS4_RECORD(renderLineWidth(width));
    glLineWidth(width);
}

void renderViewport(int x, int y, int width, int height)
{
    PS4_RECORD(renderViewport(x, y, width, height));
    s_viewport[0] = x; s_viewport[1] = y; s_viewport[2] = width; s_viewport[3] = height;
    glViewport(x, y, width, height);
}

void renderGetViewport(int* values)
{
    if (values != nullptr)
        std::memcpy(values, s_viewport, sizeof(s_viewport));
}

void renderGetMatrix(RenderMatrixQuery query, float* values)
{
    getMatrix(query, values);
}

const unsigned char* renderGetString(RenderStringQuery query)
{
    const GLenum name = query == RenderStringQuery::Vendor ? GL_VENDOR
                      : query == RenderStringQuery::Renderer ? GL_RENDERER
                      : query == RenderStringQuery::Version ? GL_VERSION
                      : GL_EXTENSIONS;
    return glGetString(name);
}

bool renderSupportsFeature(RenderFeature feature)
{
    switch (feature)
    {
        case RenderFeature::FancyFogDistance: return true;   // radial fog is a shader branch
        case RenderFeature::OcclusionQuery: return false;    // not in GLES2
        case RenderFeature::Mipmaps: return false;           // see renderTextureParameters
        case RenderFeature::AnisotropicFiltering: return hasExtension("GL_EXT_texture_filter_anisotropic");
        case RenderFeature::MultisampleAntialiasing: return false;
    }
    return false;
}

unsigned int renderGetError()
{
    return glGetError();
}

void renderFogHint(RenderHintMode) {}

// --- Matrices ------------------------------------------------------------------------

void renderMatrixMode(RenderMatrixMode mode)
{
    PS4_RECORD(renderMatrixMode(mode));
    setMatrixMode(mode);
}

void renderLoadIdentity()
{
    PS4_RECORD(renderLoadIdentity());
    currentMatrix() = Mat4::identity();
    matrixChanged();
}

void renderPushMatrix()
{
    PS4_RECORD(renderPushMatrix());
    pushMatrix();
}

void renderPopMatrix()
{
    PS4_RECORD(renderPopMatrix());
    popMatrix();
}

void renderTranslate(float x, float y, float z)
{
    PS4_RECORD(renderTranslate(x, y, z));
    currentMatrix().translate(x, y, z);
    matrixChanged();
}

void renderRotate(float angle, float x, float y, float z)
{
    PS4_RECORD(renderRotate(angle, x, y, z));
    currentMatrix().rotate(angle, x, y, z);
    matrixChanged();
}

void renderScale(float x, float y, float z)
{
    PS4_RECORD(renderScale(x, y, z));
    currentMatrix().scale(x, y, z);
    matrixChanged();
}

void renderScaleDouble(double x, double y, double z)
{
    renderScale(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

void renderFrustum(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    PS4_RECORD(renderFrustum(left, right, bottom, top, nearValue, farValue));
    currentMatrix().frustum(left, right, bottom, top, nearValue, farValue);
    matrixChanged();
}

void renderOrtho(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    PS4_RECORD(renderOrtho(left, right, bottom, top, nearValue, farValue));
    currentMatrix().ortho(left, right, bottom, top, nearValue, farValue);
    matrixChanged();
}

// --- Display lists -------------------------------------------------------------------

#if PLATFORM_DISPLAY_LISTS
int renderGenerateDisplayLists(int count)
{
    return DisplayLists::generate(count);
}

void renderDeleteDisplayLists(int first, int count)
{
    DisplayLists::remove(first, count);
}

void renderBeginDisplayList(int list)
{
    DisplayLists::begin(list);
}

void renderEndDisplayList()
{
    DisplayLists::end();
}

void renderCallDisplayList(int list)
{
    DisplayLists::call(list);
}

void renderCallDisplayLists(int count, const int* lists)
{
    DisplayLists::callMany(count, lists);
}

// GLES2 has no occlusion queries and renderSupportsFeature() says so, so the
// renderer never enables them. These keep the desktop contract ("fail open":
// available and visible) should a caller ask anyway.
void renderGenerateOcclusionQueries(int count, int* queries)
{
    for (int i = 0; queries != nullptr && i < count; ++i)
        queries[i] = s_nextQuery++;
}
void renderBeginOcclusionQuery(int) {}
void renderEndOcclusionQuery() {}
bool renderOcclusionQueryResultAvailable(int) { return true; }
unsigned int renderOcclusionQueryResult(int) { return 1u; }
#endif

#if PLATFORM_FRAMEBUFFER_READBACK
bool renderReadPixelsRgb(int x, int y, int width, int height, void* pixels)
{
    if (pixels == nullptr || width <= 0 || height <= 0)
        return false;
    // GLES2 only guarantees RGBA/UNSIGNED_BYTE readback; pack to RGB in place.
    // Row by row keeps the scratch at one row instead of a full RGBA frame.
    std::string row(static_cast<std::size_t>(width) * 4u, '\0');
    unsigned char* out = static_cast<unsigned char*>(pixels);
    for (int j = 0; j < height; ++j)
    {
        glReadPixels(x, y + j, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, &row[0]);
        for (int i = 0; i < width; ++i)
        {
            out[(static_cast<std::size_t>(j) * width + i) * 3 + 0] = static_cast<unsigned char>(row[i * 4 + 0]);
            out[(static_cast<std::size_t>(j) * width + i) * 3 + 1] = static_cast<unsigned char>(row[i * 4 + 1]);
            out[(static_cast<std::size_t>(j) * width + i) * 3 + 2] = static_cast<unsigned char>(row[i * 4 + 2]);
        }
    }
    return true;
}
#endif

bool renderCopyFramebufferToBoundTexture(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);
    return glGetError() == GL_NO_ERROR;
}

void renderSetLegacyPresentationGamma(bool)
{
}

// --- Geometry ------------------------------------------------------------------------

bool renderDrawInterleaved(const RenderInterleavedMesh& mesh)
{
    if (DisplayLists::recording())
        return DisplayLists::recordDraw(mesh);
    return drawInterleaved(mesh);
}

bool renderCaptureInterleaved(const RenderInterleavedMesh& mesh, RenderCapturedMesh& out, bool append)
{
    if (mesh.data == nullptr || mesh.stride <= 0 || mesh.count <= 0)
        return false;
    if (!append)
        out.clear();
    if (!out.empty() && (out.stride != mesh.stride || out.primitive != mesh.primitive ||
        out.positionShort != mesh.positionShort ||
        out.hasTexture != mesh.hasTexture || (mesh.hasTexture && out.texCoordOffset != mesh.texCoordOffset) ||
        out.hasColor != mesh.hasColor || (mesh.hasColor && out.colorOffset != mesh.colorOffset) ||
        out.hasNormals != mesh.hasNormals || (mesh.hasNormals && out.normalOffset != mesh.normalOffset) ||
        out.hasBrightness != mesh.hasBrightness || (mesh.hasBrightness && out.brightnessOffset != mesh.brightnessOffset)))
        return false;
    if (out.empty())
    {
        out.stride = mesh.stride; out.primitive = mesh.primitive; out.positionShort = mesh.positionShort;
        out.hasTexture = mesh.hasTexture; out.texCoordOffset = mesh.texCoordOffset;
        out.hasColor = mesh.hasColor; out.colorOffset = mesh.colorOffset;
        out.hasNormals = mesh.hasNormals; out.normalOffset = mesh.normalOffset;
        out.hasBrightness = mesh.hasBrightness; out.brightnessOffset = mesh.brightnessOffset;
    }
    const unsigned char* src = static_cast<const unsigned char*>(mesh.data) +
                               static_cast<std::size_t>(mesh.first) * static_cast<std::size_t>(mesh.stride);
    const std::size_t bytes = static_cast<std::size_t>(mesh.count) * static_cast<std::size_t>(mesh.stride);
    const std::size_t old = out.raw.size();
    out.raw.resize(old + (bytes + 3u) / 4u);
    std::memcpy(reinterpret_cast<unsigned char*>(out.raw.data()) + old * 4u, src, bytes);
    out.vertexCount += mesh.count;
    return true;
}

bool renderDrawCaptured(const RenderCapturedMesh& mesh)
{
    if (mesh.empty())
        return false;
    RenderInterleavedMesh view;
    view.data = mesh.raw.data(); view.stride = mesh.stride; view.count = mesh.vertexCount;
    view.primitive = mesh.primitive; view.positionShort = mesh.positionShort;
    view.hasTexture = mesh.hasTexture; view.texCoordOffset = mesh.texCoordOffset;
    view.hasColor = mesh.hasColor; view.colorOffset = mesh.colorOffset;
    view.hasNormals = mesh.hasNormals; view.normalOffset = mesh.normalOffset;
    view.hasBrightness = mesh.hasBrightness; view.brightnessOffset = mesh.brightnessOffset;
    return renderDrawInterleaved(view);
}
