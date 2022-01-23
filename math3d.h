//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once
#include "types.h"

// vector math 
// -------------------------------------------------------------------------------

struct Vec2
{
    float x, y;

    constexpr inline Vec2() : x(0), y(0) {}
    constexpr inline explicit Vec2(float v) : x(v), y(v) {}
    constexpr inline Vec2(float x, float y) : x(x), y(y) {}
    constexpr inline Vec2(const Vec2& v) : x(v.x), y(v.y) {}

    constexpr inline Vec2 operator = (const Vec2& v) { x = v.x; y = v.y; return *this; }

    constexpr inline Vec2 operator + (const Vec2& v) const { return {x + v.x, y + v.y}; }
    constexpr inline Vec2 operator - (const Vec2& v) const { return {x - v.x, y - v.y}; }
    constexpr inline Vec2 operator * (const Vec2& v) const { return {x * v.x, y * v.y}; }
    constexpr inline Vec2 operator * (float f) const { return { x * f, y * f }; }
    constexpr inline Vec2 operator / (float f) const { float i = 1 / f; return { x * i, y * i }; }

    constexpr inline float LengthSq() const { return x * x + y * y; }
    inline float Length() const { return sqrtf(LengthSq()); }

    inline Vec2 Rotate(float a) const { float s = sinf(a); float c = cosf(a); return Vec2(c * x + s * y, c * y - s * x); }

    constexpr inline float operator[](int i) const { return ((const float*)this)[i]; }
    inline operator const float* () const { return (const float*)this; }
};

struct Vec3
{
    float x, y, z;

    constexpr inline Vec3() : x(0), y(0), z(0) {}
    constexpr inline explicit Vec3(float v) : x(v), y(v), z(v) {}
    constexpr inline Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    constexpr inline Vec3(const Vec2& xy, float z) : x(xy.x), y(xy.y), z(z) {}
    constexpr inline Vec3(const Vec3& v) : x(v.x), y(v.y), z(v.z) {}

    constexpr inline Vec3 operator = (const Vec3& v) { x = v.x; y = v.y; z = v.z; return *this; }

    constexpr inline Vec3 operator + (const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    constexpr inline Vec3 operator - (const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    constexpr inline Vec3 operator * (const Vec3& v) const { return { x * v.x, y * v.y, z * v.z }; }
    constexpr inline Vec3 operator * (float f) const { return { x * f, y * f, z * f }; }
    constexpr inline Vec3 operator / (float f) const { float i = 1 / f; return { x * i, y * i, z * i }; }

    constexpr inline Vec3 operator % (const Vec3& v) const { return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x }; }

    constexpr inline float LengthSq() const { return x * x + y * y + z * z; }
    inline float Length() const { return sqrtf(LengthSq()); }

    constexpr inline float operator[](int i) const { return ((const float*)this)[i]; }
    inline operator const float* () const { return (const float*)this; }
};

struct Vec3P : Vec3 
{
    constexpr inline Vec3P() : Vec3() {}
    constexpr inline explicit Vec3P(float v) : Vec3(v) {}
    constexpr inline Vec3P(float x, float y, float z) : Vec3(x,y,z) {}
    constexpr inline Vec3P(const Vec2& xy, float z) : Vec3(xy, z) {}
    constexpr inline Vec3P(const Vec3& v) : Vec3(v) {}
};

struct Vec4
{
    float x, y, z, w;

    constexpr inline Vec4() : x(0), y(0), z(0), w(0) {}
    constexpr inline explicit Vec4(float v) : x(v), y(v), z(v), w(v) {}
    constexpr inline Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    constexpr inline Vec4(const Vec2& xy, float z, float w) : x(xy.x), y(xy.y), z(z), w(w) {}
    constexpr inline Vec4(const Vec2& xy, const Vec2& zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
    constexpr inline Vec4(const Vec3& xyz, float w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
    constexpr inline Vec4(const Vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}

    constexpr inline Vec4 operator=(const Vec4& v) { x = v.x; y = v.y; z = v.z; w = v.w; return *this; }

    static constexpr Vec4 FromColor(uint c)
    {
        return { ((c >> 16) & 0xff) / 255.0f,
            ((c >> 8) & 0xff) / 255.0f,
            (c & 0xff) / 255.0f,
            ((c >> 24) & 0xff) / 255.0f };
    }

    constexpr inline Vec4 operator - () const { return { -x, -y, -z, -w }; }

    constexpr inline Vec4 operator + (const Vec4& v) const { return { x + v.x, y + v.y, z + v.z, w + v.w }; }
    constexpr inline Vec4 operator - (const Vec4& v) const { return { x - v.x, y - v.y, z - v.z, w - v.w }; }
    constexpr inline Vec4 operator * (const Vec4& v) const { return { x * v.x, y * v.y, z * v.z, w * v.w }; }
    constexpr inline Vec4 operator * (float f) const { return { x * f, y * f, z * f, w * f }; }
    constexpr inline Vec4 operator / (float f) const { float i = 1 / f; return { x * i, y * i, z * i, w * i }; }

    constexpr inline float LengthSq() const { return x * x + y * y + z * z + w * w; }
    inline float Length() const { return sqrtf(LengthSq()); }

    constexpr inline float operator[](int i) const { return ((const float*)this)[i]; }
    inline operator const float* () const { return (const float*)this; }

    constexpr uint Color() const {
        return Clamp((int)(255 * z), 0, 1) |
            (Clamp((int)(255 * y), 0, 1) << 8) |
            (Clamp((int)(255 * x), 0, 1) << 16) |
            (Clamp((int)(255 * w), 0, 1) << 24);
    }

};

struct Mat22;
constexpr Vec2 operator* (const Vec2& v, const Mat22& m);

struct Mat22
{
    Vec2 i, j;

    constexpr inline explicit Mat22(bool id = true) : i(id ? 1.f : 0.f, 0), j(0, id ? 1.f : 0.f) {}
    constexpr inline Mat22(const Vec2& _i, const Vec2& _j) : i(_i), j(_j) {}
    constexpr inline Mat22(const Mat22& m) : i(m.i), j(m.j) {}

    constexpr inline Mat22 operator=(const Mat22 m) { i = m.i; j = m.j; return *this; }

    constexpr __forceinline Mat22 operator* (const Mat22 b) const
    {
        return { {
            i.x * b.i.x + i.y * b.j.x,
            i.x * b.i.y + i.y * b.j.y,         
        }, {
            j.x * b.i.x + j.y * b.j.x,
            j.x * b.i.y + j.y * b.j.y,         
        },};
    }

    constexpr inline float Determinant() const { return i.x * j.y - i.y * j.x; }

    constexpr inline Mat22 Transpose() const
    {
        return {
            { i.x, j.x },
            { i.y, j.y },            
        };
    }

    constexpr inline Mat22 InverseOrthonormal() const
    {
        Vec2 i2 = i / i.LengthSq();
        Vec2 j2 = j / j.LengthSq();
        return {
            { i2.x, j2.x },
            { i2.y, j2.y },            
        };
    }

    constexpr inline static Mat22 Scale(float s)
    {
        return {
            { s, 0},
            { 0, s},
        };
    }

    constexpr inline static Mat22 Scale(Vec2 s)
    {
        return {
            { s.x,   0},
            {   0, s.y},
        };
    }

};


struct Mat33;
constexpr Vec3 operator* (const Vec3& v, const Mat33& m);

struct Mat33
{
    Vec3 i, j, k;

    constexpr inline explicit Mat33(bool id = true) : i(id ? 1.f : 0.f, 0, 0), j(0, id ? 1.f : 0.f, 0), k(0, 0, id ? 1.f : 0.f) {}
    constexpr inline Mat33(const Vec3& _i, const Vec3& _j, const Vec3& _k) : i(_i), j(_j), k(_k) {}
    constexpr inline Mat33(const Mat33& m) : i(m.i), j(m.j), k(m.k) {}

    constexpr inline Mat33 operator=(const Mat33 m) { i = m.i; j = m.j; k = m.k; return *this; }

    constexpr __forceinline Mat33 operator* (const Mat33 b) const
    {
        return { {
            i.x * b.i.x + i.y * b.j.x + i.z * b.k.x,
            i.x * b.i.y + i.y * b.j.y + i.z * b.k.y,
            i.x * b.i.z + i.y * b.j.z + i.z * b.k.z,           
        }, {
            j.x * b.i.x + j.y * b.j.x + j.z * b.k.x,
            j.x * b.i.y + j.y * b.j.y + j.z * b.k.y,
            j.x * b.i.z + j.y * b.j.z + j.z * b.k.z,
           
        }, {
            k.x * b.i.x + k.y * b.j.x + k.z * b.k.x,
            k.x * b.i.y + k.y * b.j.y + k.z * b.k.y,
            k.x * b.i.z + k.y * b.j.z + k.z * b.k.z,           
        } };
    }

    constexpr inline float Determinant() const { 
        return i.x * (j.y * k.z - k.y * j.z) -
               i.y * (j.x * k.z - j.z * k.x) +
               i.z * (j.x * k.y - j.y * k.x);
    }

    constexpr inline Mat33 Transpose() const
    {
        return {
            { i.x, j.x, k.x },
            { i.y, j.y, k.y },
            { i.z, j.z, k.z }
        };
    }

    constexpr inline Mat33 Inverse() const 
    {        
        float invdet = 1 / Determinant();
        Mat33 minv; // inverse of matrix m
        minv.i.x = (j.y * k.z - k.y * j.z) * invdet;
        minv.i.y = (i.z * k.y - i.y * k.z) * invdet;
        minv.i.z = (i.y * j.z - i.z * j.y) * invdet;
        minv.j.x = (j.z * k.x - j.x * k.z) * invdet;
        minv.j.y = (i.x * k.z - i.z * k.x) * invdet;
        minv.j.z = (j.x * i.z - i.x * j.z) * invdet;
        minv.k.x = (j.x * k.y - k.x * j.y) * invdet;
        minv.k.y = (k.x * i.y - i.x * k.y) * invdet;
        minv.k.z = (i.x * j.y - j.x * i.y) * invdet;
        return minv;
    }


    constexpr inline Mat33 InverseOrthonormal() const
    {
        Vec3 i2 = i / i.LengthSq();
        Vec3 j2 = j / j.LengthSq();
        Vec3 k2 = k / k.LengthSq();
        return {
            { i2.x, j2.x, k2.x },
            { i2.y, j2.y, k2.y },
            { i2.z, j2.z, k2.z }           
        };
    }

    constexpr inline static Mat33 Scale(float s)
    {
        return {
            { s, 0, 0 },
            { 0, s, 0 },
            { 0, 0, s }
        };
    }

    constexpr inline static Mat33 Scale(Vec3 s)
    {
        return {
            { s.x,   0,   0 },
            {   0, s.y,   0 },
            {   0,   0, s.z }        
        };
    }

    static inline Mat33 RotX(float a)
    {
        float s = sinf(a);
        float c = cosf(a);
        return {
            { 1, 0, 0},
            { 0, c, s},
            { 0,-s, c},
        };
    }

    static inline Mat33 RotY(float a)
    {
        float s = sinf(a);
        float c = cosf(a);
        return {
            { c, 0, s},
            { 0, 1, 0},
            {-s, 0, c},
        };
    }

    static inline Mat33 RotZ(float a)
    {
        float s = sinf(a);
        float c = cosf(a);
        return {
            { c, s, 0},
            {-s, c, 0},
            { 0, 0, 1},
        };
    }
};

struct Mat44;
constexpr Vec4 operator* (const Vec4& v, const Mat44& m);

struct Mat44
{
    Vec4 i, j, k, l;

    constexpr inline Mat44(bool id=true) : i(id?1.f:0.f, 0, 0, 0), j(0, id ? 1.f : 0.f, 0, 0), k(0, 0, id ? 1.f : 0.f, 0), l(0, 0, 0, id ? 1.f : 0.f) {}
    constexpr inline Mat44(const Vec4 &_i, const Vec4 &_j, const Vec4 &_k, const Vec4 &_l) : i(_i), j(_j), k(_k), l(_l) {}
    constexpr inline Mat44(const Mat44 &m) : i(m.i), j(m.j), k(m.k), l(m.l) {}
    constexpr inline Mat44(const Mat33& m, const Vec3& t) : i(m.i, 0), j(m.j, 0), k(m.k, 0), l(t, 1) {}

    constexpr inline Mat44 operator=(const Mat44& m) { i = m.i; j = m.j; k = m.k; l = m.l; return *this; }

    constexpr __forceinline Mat44 operator* (const Mat44 b) const
    {
        return { {
            i.x * b.i.x + i.y * b.j.x + i.z * b.k.x + i.w * b.l.x,
            i.x * b.i.y + i.y * b.j.y + i.z * b.k.y + i.w * b.l.y,
            i.x * b.i.z + i.y * b.j.z + i.z * b.k.z + i.w * b.l.z,
            i.x * b.i.w + i.y * b.j.w + i.z * b.k.w + i.w * b.l.w,
        }, {
            j.x * b.i.x + j.y * b.j.x + j.z * b.k.x + j.w * b.l.x,
            j.x * b.i.y + j.y * b.j.y + j.z * b.k.y + j.w * b.l.y,
            j.x * b.i.z + j.y * b.j.z + j.z * b.k.z + j.w * b.l.z,
            j.x * b.i.w + j.y * b.j.w + j.z * b.k.w + j.w * b.l.w,
        }, {
            k.x * b.i.x + k.y * b.j.x + k.z * b.k.x + k.w * b.l.x,
            k.x * b.i.y + k.y * b.j.y + k.z * b.k.y + k.w * b.l.y,
            k.x * b.i.z + k.y * b.j.z + k.z * b.k.z + k.w * b.l.z,
            k.x * b.i.w + k.y * b.j.w + k.z * b.k.w + k.w * b.l.w,
        }, {
            l.x * b.i.x + l.y * b.j.x + l.z * b.k.x + l.w * b.l.x,
            l.x * b.i.y + l.y * b.j.y + l.z * b.k.y + l.w * b.l.y,
            l.x * b.i.z + l.y * b.j.z + l.z * b.k.z + l.w * b.l.z,
            l.x * b.i.w + l.y * b.j.w + l.z * b.k.w + l.w * b.l.w,
        } };
    }

    constexpr inline Mat44 Transpose() const
    {
        return {
            { i.x, j.x, k.x, l.x },
            { i.y, j.y, k.y, l.y },
            { i.z, j.z, k.z, l.z },
            { i.w, j.w, k.w, l.w }
        };
    }

    constexpr inline Mat44 InverseOrthonormal() const
    {
        Vec4  i2 = i / i.LengthSq();
        Vec4  j2 = j / j.LengthSq();
        Vec4  k2 = k / k.LengthSq();

        Mat44 im({
            { i2.x, j2.x, k2.x, 0 },
            { i2.y, j2.y, k2.y, 0 },
            { i2.z, j2.z, k2.z, 0 },
            l,
            });

        im.l = im.l - im.l * im;
        im.l.w = 1;
        return im;
    }


    constexpr static Mat44 Perspective(float left, float right, float top, float bottom, float front, float back)
    {
        float xx = 2.0f * (right - left);
        float yy = 2.0f * (top - bottom);
        float xz = (left + right) / (left - right);
        float yz = (top + bottom) / (bottom - top);
        float zz = back / (back - front);
        float zw = front * back / (front - back);

        return {
            {xx,  0,  0,  0},
            { 0, yy,  0,  0},
            {xz, yz, zz,  1},
            { 0,  0, zw,  0}
        };
    }

    constexpr static inline Mat44 Translate(const Vec3& loc)
    {
        return {
            { 1, 0, 0, 0},
            { 0, 1, 0, 0},
            { 0, 0, 1, 0},
            { loc,     1}
        };
    }

    constexpr static inline Mat44 Scale(float s)
    {
        return {
            { s, 0, 0, 0},
            { 0, s, 0, 0},
            { 0, 0, s, 0},
            { 0, 0, 0, 1}
        };
    }

    constexpr static inline Mat44 Scale(Vec3 s)
    {
        return {
            { s.x,   0,   0, 0},
            {   0, s.y,   0, 0},
            {   0,   0, s.z, 0},
            {   0,   0,   0, 1}
        };
    }

    static inline Mat44 RotX(float a)
    {
        float s = sinf(a);
        float c = cosf(a);
        return {
            { 1, 0, 0, 0},
            { 0, c, s, 0},
            { 0,-s, c, 0},
            { 0, 0, 0, 1}
        };
    }

    static inline Mat44 RotY(float a)
    {
        float s = sinf(a);
        float c = cosf(a);
        return {
            { c, 0, s, 0},
            { 0, 1, 0, 0},
            {-s, 0, c, 0},
            { 0, 0, 0, 1}
        };
    }

    static inline Mat44 RotZ(float a)
    {
        float s = sinf(a);
        float c = cosf(a);
        return {
            { c, s, 0, 0},
            {-s, c, 0, 0},
            { 0, 0, 1, 0},
            { 0, 0, 0, 1}
        };
    }
 };

constexpr inline float Dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
constexpr inline Vec2 Normalize(const Vec2& v) { return v / v.Length(); }
constexpr inline Vec2 Min(const Vec2& a, const Vec2& b) { return Vec2(Min(a.x, b.x), Min(a.y, b.y)); }
constexpr inline Vec2 Max(const Vec2& a, const Vec2& b) { return Vec2(Max(a.x, b.x), Max(a.y, b.y)); }
constexpr inline float MinC(const Vec2& v) { return Min(v.x, v.y); }
constexpr inline float MaxC(const Vec2& v) { return Max(v.x, v.y); }

constexpr inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr inline Vec3 Normalize(const Vec3& v) { return v / v.Length(); }
constexpr inline Vec3 Cross(const Vec3 a, const Vec3 b) { return a % b; }
constexpr inline Vec3 Min(const Vec3& a, const Vec3& b) { return Vec3(Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z)); }
constexpr inline Vec3 Max(const Vec3& a, const Vec3& b) { return Vec3(Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z)); }
constexpr inline float MinC(const Vec3& v) { return Min(v.x, Min(v.y, v.z)); }
constexpr inline float MaxC(const Vec3& v) { return Max(v.x, Max(v.y, v.z)); }

constexpr inline float Dot(const Vec4& a, const Vec4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
constexpr inline Vec4 Normalize(const Vec4& v) { return v / v.Length(); }
constexpr inline Vec4 Min(const Vec4& a, const Vec4& b) { return Vec4(Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z), Min(a.w, b.w)); }
constexpr inline Vec4 Max(const Vec4& a, const Vec4& b) { return Vec4(Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z), Max(a.w, b.w)); }
constexpr inline float MinC(const Vec4& v) { return Min(v.x, Min(v.y, Min(v.z, v.w))); }
constexpr inline float MaxC(const Vec4& v) { return Max(v.x, Max(v.y, Max(v.z, v.w))); }

constexpr inline Vec2 operator * (const Vec2& v, const Mat22& m) { return m.i * v.x + m.j * v.y; }
constexpr inline Vec3 operator * (const Vec3& v, const Mat33& m) { return m.i * v.x + m.j * v.y + m.k * v.z; }
constexpr inline Vec4 operator * (const Vec4& v, const Mat44& m) { return m.i * v.x + m.j * v.y + m.k * v.z + m.l * v.w; }
constexpr inline Vec3 operator * (const Vec3& v, const Mat44& m) { Vec4 v2 = Vec4(v, 0) * m; return { v2.x, v2.y, v2.z }; }
constexpr inline Vec3P operator * (const Vec3P& v, const Mat44& m) { Vec4 v2 = Vec4(v, 1) * m; return { v2.x, v2.y, v2.z }; }
