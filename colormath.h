//
// Copyright (C) Tammo Hinrichs 2021-2022. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once

#include "types.h"
#include "math3d.h"

constexpr inline Vec3 CIExyz(const Vec2& xy) { return { xy, 1 - (xy.x + xy.y) }; }

struct ColorSpace
{
    Vec2 r, g, b, white; // CIE xy points

    constexpr Mat33 GetRGB2xyz() const { return { CIExyz(r), CIExyz(g), CIExyz(b) }; }
    constexpr Mat33 Getxyz2RGB() const { return GetRGB2xyz().Inverse(); }

    constexpr Mat33 GetRGB2XYZ() const
    {
        Mat33 r2x = GetRGB2xyz();
        Vec3 scale = (CIExyz(white) / white.y) * r2x.Inverse();
        return { r2x.i * scale.x, r2x.j * scale.y, r2x.k * scale.z };
    }

    constexpr Mat33 GetXYZ2RGB() const { return GetRGB2XYZ().Inverse(); }

    constexpr Vec3 GetK() const { return GetRGB2XYZ().Transpose().j; }

    constexpr Mat33 GetConvertTo(const ColorSpace& to) const { return GetRGB2XYZ() * to.GetXYZ2RGB(); }
};

constexpr inline Mat33 MakeRGB2YPbPr(const Vec3& k)
{
    return Mat33
    {
        k,
        Vec3{ -k.x, -k.y, 1 - k.z } * 0.5 / (1 - k.z),
        Vec3{ 1 - k.x, -k.y, -k.z } * 0.5 / (1 - k.x),
    }.Transpose();
}

constexpr inline Mat44 MakeRGB2YUV44(const ColorSpace& space, float yMin, float yMax, float uvMin, float uvMax)
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

// color spaces
// -------------------------------------------------------------------------------

constexpr static const ColorSpace Rec709 = // also sRGB
{
    .r = { 0.640f, 0.330f }, 
    .g = { 0.300f, 0.600f },
    .b = { 0.150f, 0.060f },
    .white = { 0.3127f, 0.3290f }
};

constexpr static const ColorSpace Rec601_625 = 
{
    .r = { 0.640f, 0.330f },
    .g = { 0.290f, 0.600f },
    .b = { 0.150f, 0.060f },
    .white = { 0.3127f, 0.3290f }
};

constexpr static const ColorSpace Rec601_525 =
{
    .r = { 0.630f, 0.340f },
    .g = { 0.310f, 0.595f },
    .b = { 0.155f, 0.070f },
    .white = { 0.3127f, 0.3290f }
};

constexpr static const ColorSpace Rec2020 =
{
    .r = { 0.708f, 0.292f },
    .g = { 0.170f, 0.797f },
    .b = { 0.131f, 0.046f },
    .white = { 0.3127f, 0.3290f }
};

constexpr static const ColorSpace DCI_P3_D65 =
{
    .r = { 0.680f, 0.320f },
    .g = { 0.265f, 0.690f },
    .b = { 0.150f, 0.060f },
    .white = { 0.3127f, 0.3290f }
};

constexpr static const ColorSpace DCI_P3_DCI =
{
    .r = { 0.680f, 0.320f },
    .g = { 0.265f, 0.690f },
    .b = { 0.150f, 0.060f },
    .white = { 0.314f, 0.351f }
};

constexpr static const ColorSpace DCI_P3_D60 =
{
    .r = { 0.680f, 0.320f },
    .g = { 0.265f, 0.690f },
    .b = { 0.150f, 0.060f },
    .white = { 0.32168f, 0.33767f }
};

constexpr static const ColorSpace AdobeRGB =
{
    .r = { 0.640f, 0.330f },
    .g = { 0.210f, 0.710f },
    .b = { 0.150f, 0.060f },
    .white = { 0.3127f, 0.3290f }
};

constexpr static const ColorSpace ACES2065_1 =
{
    .r = { 0.7347f, 0.2653f },
    .g = { 0.0000f, 1.0000f },
    .b = { 0.0001f, -0.0770f },
    .white = { 0.32168f, 0.33767f }
};

constexpr static const ColorSpace ACEScg =
{
    .r = { 0.713f, 0.293f },
    .g = { 0.165f, 0.830f },
    .b = { 0.128f, 0.044f },
    .white = { 0.32168f, 0.33767f }
};



