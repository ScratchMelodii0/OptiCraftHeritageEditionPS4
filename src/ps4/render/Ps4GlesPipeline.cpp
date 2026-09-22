#ifdef PS4_PLATFORM

#include "ps4/render/Ps4GlesPipeline.h"
#include "ps4/render/Ps4GlesShader.h"
#include "ps4/system/Ps4DebugLog.h"

#include "ps4/render/Ps4Gl.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace Ps4Gles
{
namespace
{
// Streaming buffer for client-memory geometry. Orphaned (glBufferData with no
// data) whenever it wraps and at every frame start, so the driver can hand out
// fresh storage instead of stalling on draws still reading the old contents.
constexpr std::size_t kStreamBufferBytes = 16u * 1024u * 1024u;

// GL_QUADS has no GLES2 equivalent. Quads are drawn as indexed triangles
// (0,1,2)(0,2,3) through one static 16-bit index buffer covering 16384 quads;
// longer runs are split into 65536-vertex chunks with rebased attributes.
constexpr int kQuadBatchVertices = 65536;
constexpr int kQuadBatchQuads = kQuadBatchVertices / 4;

enum DirtyBits : unsigned
{
    kDirtyModelView  = 1u << 0,
    kDirtyProjection = 1u << 1,
    kDirtyTexMatrix  = 1u << 2,
    kDirtyLighting   = 1u << 3,
    kDirtyFog        = 1u << 4,
    kDirtyAlpha      = 1u << 5,
    kDirtyTexEnable  = 1u << 6,
    kDirtyAll        = 0xFFFFFFFFu,
};

struct LightState
{
    bool enabled = false;
    float direction[3] = { 0.0f, 0.0f, 1.0f };   // eye space, normalised
    float diffuse[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float ambient[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};

struct State
{
    bool initialized = false;
    ShaderProgram shader;
    GLuint streamBuffer = 0;
    std::size_t streamOffset = 0;
    GLuint quadIndexBuffer = 0;

    unsigned dirty = kDirtyAll;

    // Fixed-function switches the shader implements.
    bool texture2D[2] = { false, false };
    bool lighting = false;
    bool colorMaterial = false;
    bool fog = false;
    bool alphaTest = false;
    // Switches GLES2 still has natively; mirrored for capabilityEnabled().
    bool cullFace = false;
    bool blend = false;
    bool depthTest = false;
    bool polygonOffsetFill = false;

    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float normal[3] = { 0.0f, 0.0f, 1.0f };
    float multiTexCoord[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };

    int activeUnit = 0;
    int boundTexture[2] = { 0, 0 };

    RenderCompare alphaFunc = RenderCompare::Always;
    float alphaRef = 0.0f;

    RenderFogMode fogMode = RenderFogMode::Exp;
    bool fogRadial = false;
    float fogStart = 0.0f;
    float fogEnd = 1.0f;
    float fogDensity = 1.0f;
    float fogColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    LightState lights[2];
    float sceneAmbient[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

    RenderMatrixMode matrixMode = RenderMatrixMode::ModelView;
    MatrixStack<32> modelView;
    MatrixStack<4> projection;
    MatrixStack<4> texture[2];
};

State s;

void setDefaultLights()
{
    // GL defaults: LIGHT0 diffuse is white, every other light's is black.
    s.lights[0].diffuse[0] = s.lights[0].diffuse[1] = s.lights[0].diffuse[2] = 1.0f;
}

void markMatrixDirty()
{
    switch (s.matrixMode)
    {
        case RenderMatrixMode::ModelView: s.dirty |= kDirtyModelView; break;
        case RenderMatrixMode::Projection: s.dirty |= kDirtyProjection; break;
        case RenderMatrixMode::Texture: s.dirty |= kDirtyTexMatrix; break;
    }
}

void uploadUniforms()
{
    const ShaderProgram& p = s.shader;
    if (s.dirty & kDirtyModelView)
    {
        const Mat4& mv = s.modelView.current();
        glUniformMatrix4fv(p.modelView, 1, GL_FALSE, mv.m);
        float normalMatrix[9];
        mv.normalMatrix(normalMatrix);
        glUniformMatrix3fv(p.normalMatrix, 1, GL_FALSE, normalMatrix);
    }
    if (s.dirty & kDirtyProjection)
        glUniformMatrix4fv(p.projection, 1, GL_FALSE, s.projection.current().m);
    if (s.dirty & kDirtyTexMatrix)
    {
        glUniformMatrix4fv(p.textureMatrix0, 1, GL_FALSE, s.texture[0].current().m);
        glUniformMatrix4fv(p.textureMatrix1, 1, GL_FALSE, s.texture[1].current().m);
    }
    if (s.dirty & kDirtyTexEnable)
    {
        glUniform1f(p.texture0Enabled, s.texture2D[0] ? 1.0f : 0.0f);
        glUniform1f(p.texture1Enabled, s.texture2D[1] ? 1.0f : 0.0f);
    }
    if (s.dirty & kDirtyLighting)
    {
        glUniform1f(p.lightingEnabled, s.lighting ? 1.0f : 0.0f);
        glUniform1f(p.colorMaterial, s.colorMaterial ? 1.0f : 0.0f);
        glUniform4fv(p.sceneAmbient, 1, s.sceneAmbient);
        glUniform2f(p.lightEnabled, s.lights[0].enabled ? 1.0f : 0.0f, s.lights[1].enabled ? 1.0f : 0.0f);
        glUniform3fv(p.lightDirection0, 1, s.lights[0].direction);
        glUniform3fv(p.lightDirection1, 1, s.lights[1].direction);
        glUniform4fv(p.lightDiffuse0, 1, s.lights[0].diffuse);
        glUniform4fv(p.lightDiffuse1, 1, s.lights[1].diffuse);
        glUniform4fv(p.lightAmbient0, 1, s.lights[0].ambient);
        glUniform4fv(p.lightAmbient1, 1, s.lights[1].ambient);
    }
    if (s.dirty & kDirtyFog)
    {
        float mode = 0.0f;
        if (s.fog)
        {
            switch (s.fogMode)
            {
                case RenderFogMode::Linear: mode = 1.0f; break;
                case RenderFogMode::Exp: mode = 2.0f; break;
                case RenderFogMode::Exp2: mode = 3.0f; break;
                case RenderFogMode::EyeRadial: mode = 1.0f; break;
            }
        }
        glUniform1f(p.fogMode, mode);
        glUniform1f(p.fogEnabled, mode > 0.0f ? 1.0f : 0.0f);
        glUniform1f(p.fogRadial, s.fogRadial ? 1.0f : 0.0f);
        glUniform3f(p.fogParams, s.fogStart, s.fogEnd, s.fogDensity);
        glUniform4fv(p.fogColor, 1, s.fogColor);
    }
    if (s.dirty & kDirtyAlpha)
    {
        const float func = s.alphaTest ? static_cast<float>(static_cast<int>(s.alphaFunc))
                                       : static_cast<float>(static_cast<int>(RenderCompare::Always));
        glUniform1f(p.alphaFunc, func);
        glUniform1f(p.alphaRef, s.alphaRef);
    }
    s.dirty = 0;
}

void bindAttributes(std::size_t base, const RenderInterleavedMesh& mesh)
{
    const GLsizei stride = static_cast<GLsizei>(mesh.stride);
    auto at = [base](int offset) { return reinterpret_cast<const void*>(base + static_cast<std::size_t>(offset)); };

    glEnableVertexAttribArray(kAttrPosition);
    glVertexAttribPointer(kAttrPosition, 3, mesh.positionShort ? GL_SHORT : GL_FLOAT, GL_FALSE, stride, at(0));

    if (mesh.hasTexture)
    {
        glEnableVertexAttribArray(kAttrTexCoord0);
        glVertexAttribPointer(kAttrTexCoord0, 2, GL_FLOAT, GL_FALSE, stride, at(mesh.texCoordOffset));
    }
    else
    {
        glDisableVertexAttribArray(kAttrTexCoord0);
        glVertexAttrib2f(kAttrTexCoord0, s.multiTexCoord[0][0], s.multiTexCoord[0][1]);
    }

    if (mesh.hasColor)
    {
        glEnableVertexAttribArray(kAttrColor);
        glVertexAttribPointer(kAttrColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, at(mesh.colorOffset));
    }
    else
    {
        glDisableVertexAttribArray(kAttrColor);
        glVertexAttrib4fv(kAttrColor, s.color);
    }

    if (mesh.hasNormals)
    {
        glEnableVertexAttribArray(kAttrNormal);
        glVertexAttribPointer(kAttrNormal, 3, GL_BYTE, GL_TRUE, stride, at(mesh.normalOffset));
    }
    else
    {
        glDisableVertexAttribArray(kAttrNormal);
        glVertexAttrib3fv(kAttrNormal, s.normal);
    }

    // The lightmap coordinate is raw block/sky light (0..240) as shorts; the
    // unit-1 texture matrix Minecraft loads maps it into the 16x16 lightmap.
    if (mesh.hasBrightness)
    {
        glEnableVertexAttribArray(kAttrTexCoord1);
        glVertexAttribPointer(kAttrTexCoord1, 2, GL_SHORT, GL_FALSE, stride, at(mesh.brightnessOffset));
    }
    else
    {
        glDisableVertexAttribArray(kAttrTexCoord1);
        glVertexAttrib2f(kAttrTexCoord1, s.multiTexCoord[1][0], s.multiTexCoord[1][1]);
    }
}

// `base` is the byte offset of vertex 0 of the run inside the bound VBO.
void drawBound(std::size_t base, const RenderInterleavedMesh& mesh)
{
    uploadUniforms();

    if (mesh.primitive != RenderPrimitive::Quads)
    {
        bindAttributes(base, mesh);
        glDrawArrays(static_cast<GLenum>(renderPrimitiveValue(mesh.primitive)), 0, mesh.count);
        return;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.quadIndexBuffer);
    int remaining = mesh.count - (mesh.count % 4);
    std::size_t chunkBase = base;
    while (remaining > 0)
    {
        const int vertices = remaining < kQuadBatchVertices ? remaining : kQuadBatchVertices;
        bindAttributes(chunkBase, mesh);
        glDrawElements(GL_TRIANGLES, (vertices / 4) * 6, GL_UNSIGNED_SHORT, nullptr);
        remaining -= vertices;
        chunkBase += static_cast<std::size_t>(vertices) * static_cast<std::size_t>(mesh.stride);
    }
}

void orphanStreamBuffer()
{
    glBindBuffer(GL_ARRAY_BUFFER, s.streamBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(kStreamBufferBytes), nullptr, GL_STREAM_DRAW);
    s.streamOffset = 0;
}
}

bool initialize()
{
    if (s.initialized)
        return true;

    if (!buildShaderProgram(s.shader))
        return false;

    glGenBuffers(1, &s.streamBuffer);
    orphanStreamBuffer();

    std::vector<GLushort> indices(static_cast<std::size_t>(kQuadBatchQuads) * 6u);
    for (int q = 0; q < kQuadBatchQuads; ++q)
    {
        const GLushort v = static_cast<GLushort>(q * 4);
        GLushort* out = &indices[static_cast<std::size_t>(q) * 6u];
        out[0] = v; out[1] = static_cast<GLushort>(v + 1); out[2] = static_cast<GLushort>(v + 2);
        out[3] = v; out[4] = static_cast<GLushort>(v + 2); out[5] = static_cast<GLushort>(v + 3);
    }
    glGenBuffers(1, &s.quadIndexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.quadIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLushort)),
                 indices.data(), GL_STATIC_DRAW);

    setDefaultLights();
    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    s.dirty = kDirtyAll;
    s.initialized = true;
    return true;
}

void shutdown()
{
    if (!s.initialized)
        return;
    glDeleteBuffers(1, &s.streamBuffer);
    glDeleteBuffers(1, &s.quadIndexBuffer);
    destroyShaderProgram(s.shader);
    s = State{};
}

void setCapability(RenderCapability capability, bool enabled)
{
    const GLenum glSwitch = [capability]() -> GLenum {
        switch (capability)
        {
            case RenderCapability::CullFace: return GL_CULL_FACE;
            case RenderCapability::Blend: return GL_BLEND;
            case RenderCapability::DepthTest: return GL_DEPTH_TEST;
            case RenderCapability::PolygonOffsetFill: return GL_POLYGON_OFFSET_FILL;
            default: return 0;
        }
    }();
    if (glSwitch != 0)
    {
        if (enabled) glEnable(glSwitch); else glDisable(glSwitch);
    }

    switch (capability)
    {
        case RenderCapability::Texture2D: s.texture2D[s.activeUnit] = enabled; s.dirty |= kDirtyTexEnable; break;
        case RenderCapability::ColorMaterial: s.colorMaterial = enabled; s.dirty |= kDirtyLighting; break;
        case RenderCapability::Lighting: s.lighting = enabled; s.dirty |= kDirtyLighting; break;
        case RenderCapability::Light0: s.lights[0].enabled = enabled; s.dirty |= kDirtyLighting; break;
        case RenderCapability::Light1: s.lights[1].enabled = enabled; s.dirty |= kDirtyLighting; break;
        case RenderCapability::Fog: s.fog = enabled; s.dirty |= kDirtyFog; break;
        case RenderCapability::AlphaTest: s.alphaTest = enabled; s.dirty |= kDirtyAlpha; break;
        case RenderCapability::CullFace: s.cullFace = enabled; break;
        case RenderCapability::Blend: s.blend = enabled; break;
        case RenderCapability::DepthTest: s.depthTest = enabled; break;
        case RenderCapability::PolygonOffsetFill: s.polygonOffsetFill = enabled; break;
        // The shader always renormalises, which covers both of these.
        case RenderCapability::Normalize:
        case RenderCapability::RescaleNormal:
            break;
    }
}

bool capabilityEnabled(RenderCapability capability)
{
    switch (capability)
    {
        case RenderCapability::Texture2D: return s.texture2D[s.activeUnit];
        case RenderCapability::ColorMaterial: return s.colorMaterial;
        case RenderCapability::Lighting: return s.lighting;
        case RenderCapability::Light0: return s.lights[0].enabled;
        case RenderCapability::Light1: return s.lights[1].enabled;
        case RenderCapability::Fog: return s.fog;
        case RenderCapability::AlphaTest: return s.alphaTest;
        case RenderCapability::CullFace: return s.cullFace;
        case RenderCapability::Blend: return s.blend;
        case RenderCapability::DepthTest: return s.depthTest;
        case RenderCapability::PolygonOffsetFill: return s.polygonOffsetFill;
        case RenderCapability::Normalize:
        case RenderCapability::RescaleNormal:
            return true;
    }
    return false;
}

void setColor(float r, float g, float b, float a)
{
    s.color[0] = r; s.color[1] = g; s.color[2] = b; s.color[3] = a;
}

void setNormal(float x, float y, float z)
{
    s.normal[0] = x; s.normal[1] = y; s.normal[2] = z;
}

void setMultiTexCoord(int unit, float u, float v)
{
    if (unit < 0 || unit > 1)
        return;
    s.multiTexCoord[unit][0] = u;
    s.multiTexCoord[unit][1] = v;
}

void setActiveTextureUnit(int unit)
{
    if (unit < 0 || unit > 1)
        return;
    s.activeUnit = unit;
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
}

int activeTextureUnit()
{
    return s.activeUnit;
}

void bindTexture(int texture)
{
    s.boundTexture[s.activeUnit] = texture;
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
}

void setAlphaFunc(RenderCompare function, float reference)
{
    s.alphaFunc = function;
    s.alphaRef = reference;
    s.dirty |= kDirtyAlpha;
}

void setFogMode(RenderFogMode mode)
{
    s.fogMode = mode;
    s.dirty |= kDirtyFog;
}

void setFogRadial(bool radial)
{
    s.fogRadial = radial;
    s.dirty |= kDirtyFog;
}

void setFogParameter(RenderFogParameter parameter, float value)
{
    switch (parameter)
    {
        case RenderFogParameter::Density: s.fogDensity = value; break;
        case RenderFogParameter::Start: s.fogStart = value; break;
        case RenderFogParameter::End: s.fogEnd = value; break;
        default: return;
    }
    s.dirty |= kDirtyFog;
}

void setFogColor(const float* rgba)
{
    if (rgba == nullptr)
        return;
    std::memcpy(s.fogColor, rgba, sizeof(s.fogColor));
    s.dirty |= kDirtyFog;
}

void setLight(int light, RenderLightParameter parameter, const float* values)
{
    if (light < 0 || light > 1 || values == nullptr)
        return;
    LightState& l = s.lights[light];
    switch (parameter)
    {
        case RenderLightParameter::Ambient: std::memcpy(l.ambient, values, sizeof(l.ambient)); break;
        case RenderLightParameter::Diffuse: std::memcpy(l.diffuse, values, sizeof(l.diffuse)); break;
        case RenderLightParameter::Specular: return;   // no specular term; Minecraft sets it to 0
        case RenderLightParameter::Position:
        {
            // GL transforms the position by the modelview current at the time of
            // the call. Minecraft only uses directional lights (w == 0), so the
            // translation drops out and the direction is all that is kept.
            float eye[4];
            const float in[4] = { values[0], values[1], values[2], 0.0f };
            s.modelView.current().transform(in, eye);
            const float length = std::sqrt(eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2]);
            const float inv = length > 0.0f ? 1.0f / length : 0.0f;
            l.direction[0] = eye[0] * inv;
            l.direction[1] = eye[1] * inv;
            l.direction[2] = eye[2] * inv;
            break;
        }
    }
    s.dirty |= kDirtyLighting;
}

void setLightModelAmbient(const float* rgba)
{
    if (rgba == nullptr)
        return;
    std::memcpy(s.sceneAmbient, rgba, sizeof(s.sceneAmbient));
    s.dirty |= kDirtyLighting;
}

void setMatrixMode(RenderMatrixMode mode)
{
    s.matrixMode = mode;
}

Mat4& currentMatrix()
{
    switch (s.matrixMode)
    {
        case RenderMatrixMode::Projection: return s.projection.current();
        case RenderMatrixMode::Texture: return s.texture[s.activeUnit].current();
        case RenderMatrixMode::ModelView: break;
    }
    return s.modelView.current();
}

void matrixChanged()
{
    markMatrixDirty();
}

void pushMatrix()
{
    switch (s.matrixMode)
    {
        case RenderMatrixMode::ModelView: s.modelView.push(); break;
        case RenderMatrixMode::Projection: s.projection.push(); break;
        case RenderMatrixMode::Texture: s.texture[s.activeUnit].push(); break;
    }
}

void popMatrix()
{
    switch (s.matrixMode)
    {
        case RenderMatrixMode::ModelView: s.modelView.pop(); break;
        case RenderMatrixMode::Projection: s.projection.pop(); break;
        case RenderMatrixMode::Texture: s.texture[s.activeUnit].pop(); break;
    }
    markMatrixDirty();
}

void getMatrix(RenderMatrixQuery query, float* out16)
{
    if (out16 == nullptr)
        return;
    const Mat4* source = &s.modelView.current();
    if (query == RenderMatrixQuery::Projection)
        source = &s.projection.current();
    else if (query == RenderMatrixQuery::Texture)
        source = &s.texture[s.activeUnit].current();
    std::memcpy(out16, source->m, sizeof(source->m));
}

bool drawInterleaved(const RenderInterleavedMesh& mesh)
{
    if (!s.initialized || mesh.data == nullptr || mesh.stride <= 0 || mesh.count <= 0)
        return false;

    const std::size_t bytes = static_cast<std::size_t>(mesh.count) * static_cast<std::size_t>(mesh.stride);
    const unsigned char* source = static_cast<const unsigned char*>(mesh.data) +
                                  static_cast<std::size_t>(mesh.first) * static_cast<std::size_t>(mesh.stride);

    glBindBuffer(GL_ARRAY_BUFFER, s.streamBuffer);
    if (bytes > kStreamBufferBytes)
    {
        // Bigger than the whole ring (a huge Tessellator flush): give it a
        // dedicated orphaned allocation, then restore the ring afterwards.
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), source, GL_STREAM_DRAW);
        drawBound(0, mesh);
        orphanStreamBuffer();
        return true;
    }
    if (s.streamOffset + bytes > kStreamBufferBytes)
        orphanStreamBuffer();

    // Keep every run 16-byte aligned; some GCN fetch paths prefer it.
    const std::size_t base = s.streamOffset;
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(base), static_cast<GLsizeiptr>(bytes), source);
    s.streamOffset = (base + bytes + 15u) & ~static_cast<std::size_t>(15u);

    drawBound(base, mesh);
    return true;
}

bool drawFromBuffer(unsigned int vbo, std::size_t offset, const RenderInterleavedMesh& mesh)
{
    if (!s.initialized || vbo == 0 || mesh.stride <= 0 || mesh.count <= 0)
        return false;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    drawBound(offset, mesh);
    glBindBuffer(GL_ARRAY_BUFFER, s.streamBuffer);
    return true;
}

void endFrame()
{
    if (s.initialized)
        orphanStreamBuffer();
}
}

#endif // PS4_PLATFORM
