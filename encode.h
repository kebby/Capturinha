#pragma once

#include "types.h"
#include "graphics.h"

struct IEncode
{
    virtual ~IEncode() {}

    virtual void Init(uint sizeX, uint sizeY, uint rateNum, uint rateDen) = 0;

    virtual void SubmitFrame(RCPtr<Texture> tex) = 0;

    virtual void DuplicateFrame() = 0;

    virtual void Flush() = 0;

    virtual bool BeginGetPacket(uint8 *&data, uint &size, uint timeoutMs) = 0;
    virtual void EndGetPacket() = 0;
};

IEncode* CreateEncodeNVENC();