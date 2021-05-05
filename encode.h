//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once

#include "types.h"
#include "graphics.h"

struct CaptureConfig;

enum BufferFormat
{
    BGRA8 = 0,  // interleaved 8 bits, B,G,R,A
    NV12 = 1,   // YUV 4:2:0 8bits, Y plane followed by interleaved U,V
};

struct IEncode
{
    virtual ~IEncode() {}

    virtual void Init(uint sizeX, uint sizeY, uint rateNum, uint rateDen) = 0;

    virtual void SubmitFrame(RCPtr<Texture> tex, double time) = 0;

    virtual void DuplicateFrame() = 0;

    virtual void Flush() = 0;

    virtual bool BeginGetPacket(uint8 *&data, uint &size, uint timeoutMs, double &time) = 0;
    virtual void EndGetPacket() = 0;
};

IEncode* CreateEncodeNVENC(const CaptureConfig &cfg);