// Ps4GlesMatrix.h — the fixed-function matrix math GLES2 no longer provides.
//
// Column-major float[16], exactly the layout glGetFloatv(GL_MODELVIEW_MATRIX)
// returned on desktop GL, so renderGetMatrix() can hand these out unchanged
// (Frustum/ActiveRenderInfo read them back to build the culling planes).
// Every operation post-multiplies, matching glTranslate/glRotate/glScale.
#pragma once
#ifdef PS4_PLATFORM

#include <cmath>
#include <cstring>

namespace Ps4Gles
{
struct Mat4
{
    float m[16];

    static Mat4 identity()
    {
        Mat4 r;
        std::memset(r.m, 0, sizeof(r.m));
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    // this = this * rhs
    void multiply(const Mat4& rhs)
    {
        float out[16];
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                out[c * 4 + r] = m[0 * 4 + r] * rhs.m[c * 4 + 0] +
                                 m[1 * 4 + r] * rhs.m[c * 4 + 1] +
                                 m[2 * 4 + r] * rhs.m[c * 4 + 2] +
                                 m[3 * 4 + r] * rhs.m[c * 4 + 3];
        std::memcpy(m, out, sizeof(out));
    }

    void translate(float x, float y, float z)
    {
        // Post-multiplying by a translation only touches the last column.
        for (int r = 0; r < 4; ++r)
            m[12 + r] += m[r] * x + m[4 + r] * y + m[8 + r] * z;
    }

    void scale(float x, float y, float z)
    {
        for (int r = 0; r < 4; ++r)
        {
            m[r] *= x;
            m[4 + r] *= y;
            m[8 + r] *= z;
        }
    }

    // glRotatef: angle in degrees around (x, y, z), which need not be unit.
    void rotate(float angleDegrees, float x, float y, float z)
    {
        const float length = std::sqrt(x * x + y * y + z * z);
        if (length <= 0.0f)
            return;
        x /= length; y /= length; z /= length;
        const float radians = angleDegrees * 0.017453292519943295f;
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        const float t = 1.0f - c;

        Mat4 rot = identity();
        rot.m[0] = x * x * t + c;     rot.m[4] = x * y * t - z * s; rot.m[8]  = x * z * t + y * s;
        rot.m[1] = y * x * t + z * s; rot.m[5] = y * y * t + c;     rot.m[9]  = y * z * t - x * s;
        rot.m[2] = x * z * t - y * s; rot.m[6] = y * z * t + x * s; rot.m[10] = z * z * t + c;
        multiply(rot);
    }

    void frustum(double l, double r, double b, double t, double n, double f)
    {
        Mat4 p;
        std::memset(p.m, 0, sizeof(p.m));
        p.m[0] = static_cast<float>(2.0 * n / (r - l));
        p.m[5] = static_cast<float>(2.0 * n / (t - b));
        p.m[8] = static_cast<float>((r + l) / (r - l));
        p.m[9] = static_cast<float>((t + b) / (t - b));
        p.m[10] = static_cast<float>(-(f + n) / (f - n));
        p.m[11] = -1.0f;
        p.m[14] = static_cast<float>(-2.0 * f * n / (f - n));
        multiply(p);
    }

    void ortho(double l, double r, double b, double t, double n, double f)
    {
        Mat4 p = identity();
        p.m[0] = static_cast<float>(2.0 / (r - l));
        p.m[5] = static_cast<float>(2.0 / (t - b));
        p.m[10] = static_cast<float>(-2.0 / (f - n));
        p.m[12] = static_cast<float>(-(r + l) / (r - l));
        p.m[13] = static_cast<float>(-(t + b) / (t - b));
        p.m[14] = static_cast<float>(-(f + n) / (f - n));
        multiply(p);
    }

    // (x, y, z, w) -> this * v
    void transform(const float in[4], float out[4]) const
    {
        for (int r = 0; r < 4; ++r)
            out[r] = m[r] * in[0] + m[4 + r] * in[1] + m[8 + r] * in[2] + m[12 + r] * in[3];
    }

    // Inverse-transpose of the upper 3x3, column-major float[9]: the GL normal
    // matrix. Falls back to the plain 3x3 for a singular matrix (a zero scale
    // squashes the geometry anyway, so the lighting of it cannot matter).
    void normalMatrix(float out[9]) const
    {
        const float a = m[0], b = m[4], c = m[8];
        const float d = m[1], e = m[5], f = m[9];
        const float g = m[2], h = m[6], i = m[10];
        const float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
        const float det = a * A + b * B + c * C;
        if (std::fabs(det) < 1e-12f)
        {
            out[0] = a; out[1] = d; out[2] = g;
            out[3] = b; out[4] = e; out[5] = h;
            out[6] = c; out[7] = f; out[8] = i;
            return;
        }
        const float invDet = 1.0f / det;
        // (M^-1)^T is the cofactor matrix / det; out[col * 3 + row] = cof[row][col].
        out[0] = A * invDet;
        out[1] = -(b * i - c * h) * invDet;
        out[2] = (b * f - c * e) * invDet;
        out[3] = B * invDet;
        out[4] = (a * i - c * g) * invDet;
        out[5] = -(a * f - c * d) * invDet;
        out[6] = C * invDet;
        out[7] = -(a * h - b * g) * invDet;
        out[8] = (a * e - b * d) * invDet;
    }
};

// glPushMatrix/glPopMatrix. Depths are the desktop GL minimums (32 modelview,
// 2 projection/texture); overflow and underflow are ignored, as GL does after
// raising GL_STACK_OVERFLOW/UNDERFLOW, instead of corrupting memory.
template <int Depth>
struct MatrixStack
{
    Mat4 entries[Depth];
    int top = 0;

    MatrixStack() { entries[0] = Mat4::identity(); }
    Mat4& current() { return entries[top]; }
    const Mat4& current() const { return entries[top]; }
    bool push()
    {
        if (top + 1 >= Depth)
            return false;
        entries[top + 1] = entries[top];
        ++top;
        return true;
    }
    bool pop()
    {
        if (top == 0)
            return false;
        --top;
        return true;
    }
};
}

#endif // PS4_PLATFORM
