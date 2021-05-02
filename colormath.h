//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once

#include "types.h"
#include "math3d.h"

constexpr Vec3 CIExyz(const Vec2& xy) { return { xy, 1 - (xy.x + xy.y) }; }
constexpr Mat33 MakeRGB2XYZ(const Vec2& r, const Vec2& g, const Vec2& b) { return Mat33(CIExyz(r), CIExyz(g), CIExyz(b)); }
constexpr Vec3 CIExy2XYZ(const Vec2& xy) { return CIExyz(xy) / xy.y; }

struct ColorSpace
{
    Vec2 r, g, b, white; // CIE xy points

    constexpr Vec3 GetK() const
    {
        return Vec3 { r.y, g.y, b.y } * (CIExy2XYZ(white) * MakeRGB2XYZ(r, g, b).Inverse());
    }
};

constexpr Mat33 MakeRGB2YPbPr(const Vec3& k)
{
    return Mat33
    {
        k,
        Vec3{ -k.x, -k.y, 1 - k.z } * 0.5 / (1 - k.z),
        Vec3{ 1 - k.x, -k.y, -k.z } * 0.5 / (1 - k.x),
    }.Transpose();
}

constexpr Mat44 MakeRGB2YUV44(const ColorSpace& space, float yMin, float yMax, float uvMin, float uvMax)
{
    Mat33 rgb2ypp = MakeRGB2YPbPr(space.GetK());
    Vec3 scale = { yMax - yMin, uvMax - uvMin, uvMax - uvMin };
    return
    {
        { rgb2ypp.i * scale, 0 },
        { rgb2ypp.j * scale, 0 },
        { rgb2ypp.k * scale, 0 },
        { yMin, (uvMin + uvMax) * 0.5f, (uvMin + uvMax) * 0.5f, 1 }
    };
}

constexpr static const ColorSpace Rec709 = // also sRGB
{
    .r = { 0.640f, 0.330f }, 
    .g = { 0.300f, 0.600f },
    .b = { 0.150f, 0.060f },
    .white = { 0.3127f, 0.3290f }
};
