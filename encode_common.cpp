//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#include "encode.h"

FormatInfo GetFormatInfo(IEncode::BufferFormat fmt, uint sizeX, uint sizeY)
{
    FormatInfo info = {};
    switch (fmt)
    {
    case IEncode::BufferFormat::BGRA8:
        info.pitch = 4 * sizeX;
        info.lines = sizeY;
        info.amp = 255.0f;
        break;
    case IEncode::BufferFormat::NV12:
        info.pitch = sizeX;
        info.lines = sizeY + sizeY / 2;
        info.amp = 255.0f;
        info.ymin = info.uvmin = 16.f / 255.f;
        info.ymax = 235.f / 255.f;
        info.uvmax = 240.f / 255.f;
        break;
    case IEncode::BufferFormat::YUV444_8:
        info.pitch = sizeX;
        info.lines = 3 * sizeY;
        info.amp = 255.0f;
        info.ymin = info.uvmin = 16.f / 255.f;
        info.ymax = 235.f / 255.f;
        info.uvmax = 240.f / 255.f;
        break;
    case IEncode::BufferFormat::YUV420_16:
        info.pitch = 2 * sizeX;
        info.lines = sizeY + sizeY / 2;
        info.amp = 65535.0f;
        info.ymin = info.uvmin = 64.f / 1023.f;
        info.ymax = 940.f / 1023.f;
        info.uvmax = 960.f / 1023.f;
        break;
    case IEncode::BufferFormat::YUV444_16:
        info.pitch = 2 * sizeX;
        info.lines = 3 * sizeY;
        info.amp = 65535.0f;
        info.ymin = info.uvmin = 64.f / 1023.f;
        info.ymax = 940.f / 1023.f;
        info.uvmax = 960.f / 1023.f;
        break;
    }
    return info;
}