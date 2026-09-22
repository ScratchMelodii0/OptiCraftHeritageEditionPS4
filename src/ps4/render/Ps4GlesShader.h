// Ps4GlesShader.h — the fixed-function emulation program.
//
// Piglet only compiles GLSL at runtime when the shader compiler module
// (libSceShaccVSH) is loaded; Ps4Modules starts it when the firmware has it.
// Without it, a precompiled binary is loaded from /app0/shaders/ (see the
// README section on shaders), so the port runs on either setup.
#pragma once
#ifdef PS4_PLATFORM

namespace Ps4Gles
{
enum Attribute : unsigned int
{
    kAttrPosition = 0,
    kAttrTexCoord0 = 1,
    kAttrColor = 2,
    kAttrNormal = 3,
    kAttrTexCoord1 = 4,
};

struct ShaderProgram
{
    unsigned int program = 0;

    int modelView = -1;
    int projection = -1;
    int normalMatrix = -1;
    int textureMatrix0 = -1;
    int textureMatrix1 = -1;

    int sampler0 = -1;
    int sampler1 = -1;
    int texture0Enabled = -1;
    int texture1Enabled = -1;

    int lightingEnabled = -1;
    int colorMaterial = -1;
    int sceneAmbient = -1;
    int lightEnabled = -1;       // vec2
    int lightDirection0 = -1;
    int lightDirection1 = -1;
    int lightDiffuse0 = -1;
    int lightDiffuse1 = -1;
    int lightAmbient0 = -1;
    int lightAmbient1 = -1;

    int fogMode = -1;            // vertex: 0 off, 1 linear, 2 exp, 3 exp2
    int fogEnabled = -1;         // fragment: mix toward fog colour
    int fogRadial = -1;
    int fogParams = -1;          // start, end, density
    int fogColor = -1;

    int alphaFunc = -1;          // RenderCompare as float, 7 = Always/disabled
    int alphaRef = -1;
};

// Builds the program; false with the reason logged if neither runtime
// compilation nor a precompiled binary is available.
bool buildShaderProgram(ShaderProgram& out);
void destroyShaderProgram(ShaderProgram& program);
}

#endif // PS4_PLATFORM
