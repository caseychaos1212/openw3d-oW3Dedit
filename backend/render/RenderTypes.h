#pragma once

#include <algorithm>
#include <cmath>

namespace OW3D::Render {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct Mat4 {
    // Row-major matrix.
    float m[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    static Mat4 Identity() {
        return {};
    }
};

inline float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float Length(const Vec3& v) {
    return std::sqrt(Dot(v, v));
}

inline Vec3 Normalize(const Vec3& v) {
    const float len = Length(v);
    if (len <= 1e-6f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    const float invLen = 1.0f / len;
    return { v.x * invLen, v.y * invLen, v.z * invLen };
}

inline Vec3 operator+(const Vec3& a, const Vec3& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline Vec3 operator*(const Vec3& v, float s) {
    return { v.x * s, v.y * s, v.z * s };
}

inline Mat4 Multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out.m[r * 4 + c] =
                a.m[r * 4 + 0] * b.m[0 * 4 + c] +
                a.m[r * 4 + 1] * b.m[1 * 4 + c] +
                a.m[r * 4 + 2] * b.m[2 * 4 + c] +
                a.m[r * 4 + 3] * b.m[3 * 4 + c];
        }
    }
    return out;
}

inline Mat4 Translation(const Vec3& t) {
    Mat4 out = Mat4::Identity();
    out.m[12] = t.x;
    out.m[13] = t.y;
    out.m[14] = t.z;
    return out;
}

inline Mat4 Scale(float s) {
    Mat4 out = Mat4::Identity();
    out.m[0] = s;
    out.m[5] = s;
    out.m[10] = s;
    return out;
}

inline Mat4 QuaternionToMatrix(float x, float y, float z, float w) {
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    Mat4 out = Mat4::Identity();
    out.m[0] = 1.0f - 2.0f * (yy + zz);
    out.m[1] = 2.0f * (xy + wz);
    out.m[2] = 2.0f * (xz - wy);

    out.m[4] = 2.0f * (xy - wz);
    out.m[5] = 1.0f - 2.0f * (xx + zz);
    out.m[6] = 2.0f * (yz + wx);

    out.m[8] = 2.0f * (xz + wy);
    out.m[9] = 2.0f * (yz - wx);
    out.m[10] = 1.0f - 2.0f * (xx + yy);
    return out;
}

inline Mat4 TransformFromTranslationRotation(
    const Vec3& translation,
    float qx,
    float qy,
    float qz,
    float qw)
{
    Mat4 out = QuaternionToMatrix(qx, qy, qz, qw);
    out.m[12] = translation.x;
    out.m[13] = translation.y;
    out.m[14] = translation.z;
    return out;
}

inline Vec3 TransformPoint(const Mat4& m, const Vec3& p) {
    return {
        p.x * m.m[0] + p.y * m.m[4] + p.z * m.m[8] + m.m[12],
        p.x * m.m[1] + p.y * m.m[5] + p.z * m.m[9] + m.m[13],
        p.x * m.m[2] + p.y * m.m[6] + p.z * m.m[10] + m.m[14]
    };
}

inline float DegToRad(float degrees) {
    return degrees * 0.0174532925199432957692f;
}

inline Mat4 PerspectiveFovLH(float fovYRadians, float aspect, float zNear, float zFar) {
    Mat4 out{};
    const float yScale = 1.0f / std::tan(fovYRadians * 0.5f);
    const float xScale = yScale / std::max(0.0001f, aspect);

    out.m[0] = xScale;
    out.m[5] = yScale;
    out.m[10] = zFar / (zFar - zNear);
    out.m[11] = 1.0f;
    out.m[14] = (-zNear * zFar) / (zFar - zNear);
    out.m[15] = 0.0f;
    return out;
}

inline Mat4 LookAtLH(const Vec3& eye, const Vec3& at, const Vec3& up) {
    const Vec3 zaxis = Normalize(at - eye);
    const Vec3 xaxis = Normalize(Cross(up, zaxis));
    const Vec3 yaxis = Cross(zaxis, xaxis);

    Mat4 out{};
    out.m[0] = xaxis.x;
    out.m[1] = yaxis.x;
    out.m[2] = zaxis.x;
    out.m[3] = 0.0f;

    out.m[4] = xaxis.y;
    out.m[5] = yaxis.y;
    out.m[6] = zaxis.y;
    out.m[7] = 0.0f;

    out.m[8] = xaxis.z;
    out.m[9] = yaxis.z;
    out.m[10] = zaxis.z;
    out.m[11] = 0.0f;

    out.m[12] = -Dot(xaxis, eye);
    out.m[13] = -Dot(yaxis, eye);
    out.m[14] = -Dot(zaxis, eye);
    out.m[15] = 1.0f;
    return out;
}

inline Mat4 Inverse(const Mat4& in) {
    const float* m = in.m;
    float inv[16];

    inv[0] = m[5] * m[10] * m[15]
        - m[5] * m[11] * m[14]
        - m[9] * m[6] * m[15]
        + m[9] * m[7] * m[14]
        + m[13] * m[6] * m[11]
        - m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15]
        + m[4] * m[11] * m[14]
        + m[8] * m[6] * m[15]
        - m[8] * m[7] * m[14]
        - m[12] * m[6] * m[11]
        + m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15]
        - m[4] * m[11] * m[13]
        - m[8] * m[5] * m[15]
        + m[8] * m[7] * m[13]
        + m[12] * m[5] * m[11]
        - m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14]
        + m[4] * m[10] * m[13]
        + m[8] * m[5] * m[14]
        - m[8] * m[6] * m[13]
        - m[12] * m[5] * m[10]
        + m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15]
        + m[1] * m[11] * m[14]
        + m[9] * m[2] * m[15]
        - m[9] * m[3] * m[14]
        - m[13] * m[2] * m[11]
        + m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15]
        - m[0] * m[11] * m[14]
        - m[8] * m[2] * m[15]
        + m[8] * m[3] * m[14]
        + m[12] * m[2] * m[11]
        - m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15]
        + m[0] * m[11] * m[13]
        + m[8] * m[1] * m[15]
        - m[8] * m[3] * m[13]
        - m[12] * m[1] * m[11]
        + m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14]
        - m[0] * m[10] * m[13]
        - m[8] * m[1] * m[14]
        + m[8] * m[2] * m[13]
        + m[12] * m[1] * m[10]
        - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15]
        - m[1] * m[7] * m[14]
        - m[5] * m[2] * m[15]
        + m[5] * m[3] * m[14]
        + m[13] * m[2] * m[7]
        - m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15]
        + m[0] * m[7] * m[14]
        + m[4] * m[2] * m[15]
        - m[4] * m[3] * m[14]
        - m[12] * m[2] * m[7]
        + m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15]
        - m[0] * m[7] * m[13]
        - m[4] * m[1] * m[15]
        + m[4] * m[3] * m[13]
        + m[12] * m[1] * m[7]
        - m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14]
        + m[0] * m[6] * m[13]
        + m[4] * m[1] * m[14]
        - m[4] * m[2] * m[13]
        - m[12] * m[1] * m[6]
        + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11]
        + m[1] * m[7] * m[10]
        + m[5] * m[2] * m[11]
        - m[5] * m[3] * m[10]
        - m[9] * m[2] * m[7]
        + m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11]
        - m[0] * m[7] * m[10]
        - m[4] * m[2] * m[11]
        + m[4] * m[3] * m[10]
        + m[8] * m[2] * m[7]
        - m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11]
        + m[0] * m[7] * m[9]
        + m[4] * m[1] * m[11]
        - m[4] * m[3] * m[9]
        - m[8] * m[1] * m[7]
        + m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10]
        - m[0] * m[6] * m[9]
        - m[4] * m[1] * m[10]
        + m[4] * m[2] * m[9]
        + m[8] * m[1] * m[6]
        - m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(det) < 1e-8f) {
        return Mat4::Identity();
    }

    det = 1.0f / det;
    Mat4 out{};
    for (int i = 0; i < 16; ++i) {
        out.m[i] = inv[i] * det;
    }
    return out;
}

} // namespace OW3D::Render

