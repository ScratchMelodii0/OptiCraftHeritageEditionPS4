#ifdef PS4_PLATFORM

#include "ps4/render/Ps4GlesShader.h"
#include "ps4/system/Ps4DebugLog.h"

#include "ps4/render/Ps4Gl.h"

#include <cstdio>
#include <vector>

namespace
{
// GLSL ES 1.00. Lighting and fog are evaluated per vertex, as the fixed-function
// pipeline did; the fragment stage only textures, fogs and alpha-tests.
const char* kVertexSource = R"GLSL(
precision highp float;

attribute vec3 a_position;
attribute vec2 a_texCoord0;
attribute vec4 a_color;
attribute vec3 a_normal;
attribute vec2 a_texCoord1;

uniform mat4 u_modelView;
uniform mat4 u_projection;
uniform mat3 u_normalMatrix;
uniform mat4 u_textureMatrix0;
uniform mat4 u_textureMatrix1;

uniform float u_lightingEnabled;
uniform float u_colorMaterial;
uniform vec4 u_sceneAmbient;
uniform vec2 u_lightEnabled;
uniform vec3 u_lightDirection0;
uniform vec3 u_lightDirection1;
uniform vec4 u_lightDiffuse0;
uniform vec4 u_lightDiffuse1;
uniform vec4 u_lightAmbient0;
uniform vec4 u_lightAmbient1;

uniform float u_fogMode;
uniform float u_fogRadial;
uniform vec3 u_fogParams;

varying vec4 v_color;
varying vec2 v_texCoord0;
varying vec2 v_texCoord1;
varying float v_fogFactor;

void main()
{
    vec4 eye = u_modelView * vec4(a_position, 1.0);
    gl_Position = u_projection * eye;

    v_texCoord0 = (u_textureMatrix0 * vec4(a_texCoord0, 0.0, 1.0)).xy;
    v_texCoord1 = (u_textureMatrix1 * vec4(a_texCoord1, 0.0, 1.0)).xy;

    vec4 color = a_color;
    if (u_lightingEnabled > 0.5)
    {
        // GL defaults when GL_COLOR_MATERIAL is off: ambient 0.2, diffuse 0.8.
        vec4 matAmbient = u_colorMaterial > 0.5 ? a_color : vec4(0.2, 0.2, 0.2, 1.0);
        vec4 matDiffuse = u_colorMaterial > 0.5 ? a_color : vec4(0.8, 0.8, 0.8, 1.0);
        vec3 n = normalize(u_normalMatrix * a_normal);
        vec3 lit = u_sceneAmbient.rgb * matAmbient.rgb;
        lit += u_lightEnabled.x * (u_lightAmbient0.rgb * matAmbient.rgb +
               u_lightDiffuse0.rgb * matDiffuse.rgb * max(dot(n, u_lightDirection0), 0.0));
        lit += u_lightEnabled.y * (u_lightAmbient1.rgb * matAmbient.rgb +
               u_lightDiffuse1.rgb * matDiffuse.rgb * max(dot(n, u_lightDirection1), 0.0));
        color = vec4(clamp(lit, 0.0, 1.0), matDiffuse.a);
    }
    v_color = color;

    float distance = u_fogRadial > 0.5 ? length(eye.xyz) : abs(eye.z);
    float fog = 1.0;
    if (u_fogMode > 2.5)
    {
        float d = u_fogParams.z * distance;
        fog = exp(-d * d);
    }
    else if (u_fogMode > 1.5)
        fog = exp(-u_fogParams.z * distance);
    else if (u_fogMode > 0.5)
        fog = (u_fogParams.y - distance) / max(u_fogParams.y - u_fogParams.x, 0.0001);
    v_fogFactor = clamp(fog, 0.0, 1.0);
}
)GLSL";

const char* kFragmentSource = R"GLSL(
precision mediump float;

uniform sampler2D u_sampler0;
uniform sampler2D u_sampler1;
uniform float u_texture0Enabled;
uniform float u_texture1Enabled;
uniform vec4 u_fogColor;
// Separate from the vertex stage's u_fogMode: a uniform shared by both
// stages must match precision, and this stage defaults to mediump.
uniform float u_fogEnabled;
uniform float u_alphaFunc;
uniform float u_alphaRef;

varying vec4 v_color;
varying vec2 v_texCoord0;
varying vec2 v_texCoord1;
varying float v_fogFactor;

void main()
{
    vec4 color = v_color;
    if (u_texture0Enabled > 0.5)
        color *= texture2D(u_sampler0, v_texCoord0);
    if (u_texture1Enabled > 0.5)
        color.rgb *= texture2D(u_sampler1, v_texCoord1).rgb;

    // RenderCompare order: Never, Less, Equal, LessEqual, Greater, NotEqual,
    // GreaterEqual, Always. The test runs before fog, as in GL.
    float a = color.a;
    float f = u_alphaFunc;
    bool pass = true;
    if (f < 0.5) pass = false;
    else if (f < 1.5) pass = a < u_alphaRef;
    else if (f < 2.5) pass = a == u_alphaRef;
    else if (f < 3.5) pass = a <= u_alphaRef;
    else if (f < 4.5) pass = a > u_alphaRef;
    else if (f < 5.5) pass = a != u_alphaRef;
    else if (f < 6.5) pass = a >= u_alphaRef;
    if (!pass)
        discard;

    if (u_fogEnabled > 0.5)
        color.rgb = mix(u_fogColor.rgb, color.rgb, v_fogFactor);
    gl_FragColor = color;
}
)GLSL";

bool readFile(const char* path, std::vector<unsigned char>& out)
{
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr)
        return false;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0)
    {
        std::fclose(file);
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    const bool ok = std::fread(out.data(), 1, out.size(), file) == out.size();
    std::fclose(file);
    return ok;
}

void logShaderInfo(GLuint shader, const char* what)
{
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1)
    {
        Ps4DebugLog::printf("[ps4-gles] %s failed (no info log)\n", what);
        return;
    }
    std::vector<char> log(static_cast<std::size_t>(length));
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    Ps4DebugLog::printf("[ps4-gles] %s failed:\n%s\n", what, log.data());
}

// Runtime GLSL compilation. Fails cleanly (GL_INVALID_OPERATION or a
// "compiler not supported" log) when libSceShaccVSH is not loaded.
GLuint compileSource(GLenum type, const char* source, const char* what)
{
    GLboolean hasCompiler = GL_FALSE;
    glGetBooleanv(GL_SHADER_COMPILER, &hasCompiler);
    if (hasCompiler == GL_FALSE)
        return 0;

    const GLuint shader = glCreateShader(type);
    if (shader == 0)
        return 0;
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        logShaderInfo(shader, what);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Precompiled PSSL binary (glShaderBinary format 0 on Piglet).
GLuint loadBinary(GLenum type, const char* path)
{
    std::vector<unsigned char> blob;
    if (!readFile(path, blob))
        return 0;
    const GLuint shader = glCreateShader(type);
    if (shader == 0)
        return 0;
    glShaderBinary(1, &shader, 0, blob.data(), static_cast<GLsizei>(blob.size()));
    if (glGetError() != GL_NO_ERROR)
    {
        Ps4DebugLog::printf("[ps4-gles] glShaderBinary(%s) rejected\n", path);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint obtainShader(GLenum type, const char* source, const char* binaryPath, const char* what)
{
    GLuint shader = compileSource(type, source, what);
    if (shader != 0)
        return shader;
    shader = loadBinary(type, binaryPath);
    if (shader != 0)
        Ps4DebugLog::printf("[ps4-gles] %s: using precompiled %s\n", what, binaryPath);
    return shader;
}
}

namespace Ps4Gles
{
bool buildShaderProgram(ShaderProgram& out)
{
    const GLuint vertex = obtainShader(GL_VERTEX_SHADER, kVertexSource,
                                       "/app0/shaders/ffp_vs.sb", "vertex shader");
    const GLuint fragment = obtainShader(GL_FRAGMENT_SHADER, kFragmentSource,
                                         "/app0/shaders/ffp_ps.sb", "fragment shader");
    if (vertex == 0 || fragment == 0)
    {
        Ps4DebugLog::printf("[ps4-gles] no usable shaders: runtime compiler unavailable and "
                            "/app0/shaders/ffp_{vs,ps}.sb missing\n");
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, kAttrPosition, "a_position");
    glBindAttribLocation(program, kAttrTexCoord0, "a_texCoord0");
    glBindAttribLocation(program, kAttrColor, "a_color");
    glBindAttribLocation(program, kAttrNormal, "a_normal");
    glBindAttribLocation(program, kAttrTexCoord1, "a_texCoord1");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE)
    {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<std::size_t>(length > 1 ? length : 1), '\0');
        if (length > 1)
            glGetProgramInfoLog(program, length, nullptr, log.data());
        Ps4DebugLog::printf("[ps4-gles] program link failed:\n%s\n", log.data());
        glDeleteProgram(program);
        return false;
    }

    out.program = program;
    auto uniform = [program](const char* name) { return glGetUniformLocation(program, name); };
    out.modelView = uniform("u_modelView");
    out.projection = uniform("u_projection");
    out.normalMatrix = uniform("u_normalMatrix");
    out.textureMatrix0 = uniform("u_textureMatrix0");
    out.textureMatrix1 = uniform("u_textureMatrix1");
    out.sampler0 = uniform("u_sampler0");
    out.sampler1 = uniform("u_sampler1");
    out.texture0Enabled = uniform("u_texture0Enabled");
    out.texture1Enabled = uniform("u_texture1Enabled");
    out.lightingEnabled = uniform("u_lightingEnabled");
    out.colorMaterial = uniform("u_colorMaterial");
    out.sceneAmbient = uniform("u_sceneAmbient");
    out.lightEnabled = uniform("u_lightEnabled");
    out.lightDirection0 = uniform("u_lightDirection0");
    out.lightDirection1 = uniform("u_lightDirection1");
    out.lightDiffuse0 = uniform("u_lightDiffuse0");
    out.lightDiffuse1 = uniform("u_lightDiffuse1");
    out.lightAmbient0 = uniform("u_lightAmbient0");
    out.lightAmbient1 = uniform("u_lightAmbient1");
    out.fogMode = uniform("u_fogMode");
    out.fogRadial = uniform("u_fogRadial");
    out.fogParams = uniform("u_fogParams");
    out.fogEnabled = uniform("u_fogEnabled");
    out.fogColor = uniform("u_fogColor");
    out.alphaFunc = uniform("u_alphaFunc");
    out.alphaRef = uniform("u_alphaRef");

    glUseProgram(program);
    glUniform1i(out.sampler0, 0);
    glUniform1i(out.sampler1, 1);
    return true;
}

void destroyShaderProgram(ShaderProgram& program)
{
    if (program.program != 0)
        glDeleteProgram(program.program);
    program = ShaderProgram{};
}
}

#endif // PS4_PLATFORM
