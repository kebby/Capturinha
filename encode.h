//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once

#include "types.h"
#include "graphics.h"

struct CaptureConfig;

struct IEncode
{
    enum class BufferFormat
    {
        BGRA8,      // interleaved 8 bits, B,G,R,A
        NV12,       // YUV 4:2:0 16 bits, Y plane followed by interleaved U,V
        YUV444_8,   // Planar YUV 4:4:4 8 bits
        YUV420_16,  // YUV 4:2:0 16 bits, Y plane followed by interleaved U,V
        YUV444_16,  // Planar YUV 4:4:4 16 bits
    };

    virtual ~IEncode() {}

    virtual BufferFormat GetBufferFormat() = 0;

    virtual void Init(uint sizeX, uint sizeY, uint rateNum, uint rateDen, RCPtr<GpuByteBuffer> buffer) = 0;

    virtual void SubmitFrame(double time) = 0;

    virtual void DuplicateFrame() = 0;

    virtual void Flush() = 0;

    virtual bool BeginGetPacket(uint8 *&data, uint &size, uint timeoutMs, double &time) = 0;
    virtual void EndGetPacket() = 0;
};

IEncode* CreateEncodeNVENC(const CaptureConfig &cfg, bool isHdr);

struct FormatInfo
{
    uint pitch;
    uint lines;
    float amp;
    float ymin, ymax, uvmin, uvmax;
};

FormatInfo GetFormatInfo(IEncode::BufferFormat fmt, uint sizeX, uint sizeY);