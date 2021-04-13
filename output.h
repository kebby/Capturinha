#pragma once

#include "types.h"
#include "audiocapture.h"

class IOutput
{
public:

    virtual ~IOutput() {}

    virtual void SubmitVideoPacket(const uint8* data, uint size) = 0;

    virtual void SetAudioDelay(double delaySec) = 0;
    virtual void SubmitAudio(const uint8* data, uint size) = 0;
};

struct OutputPara
{
    const char* filename;
    uint SizeX;
    uint SizeY;
    uint RateNum;
    uint RateDen;
    AudioInfo Audio;
};

IOutput* CreateOutputLibAV(const OutputPara &para);
