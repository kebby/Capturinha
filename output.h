//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once

#include "types.h"
#include "audiocapture.h"

struct CaptureConfig;

class IOutput
{
public:
    virtual ~IOutput() {}

    virtual void SubmitVideoPacket(const uint8* data, uint size) = 0;

    virtual void SubmitAudio(const uint8* data, uint size) = 0;
};

struct OutputPara
{
    String filename;
    uint SizeX;
    uint SizeY;
    uint RateNum;
    uint RateDen;

    AudioInfo Audio;

    const CaptureConfig* CConfig;
};

IOutput* CreateOutputLibAV(const OutputPara &para);
