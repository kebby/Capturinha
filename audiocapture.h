//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once

struct CaptureConfig;

enum class AudioFormat
{
    None,
    I16,
    F32,
};

struct AudioInfo
{
    AudioFormat Format;
    uint Channels;
    uint SampleRate;
    uint BytesPerSample;
};

class IAudioCapture
{
public:
    virtual ~IAudioCapture() {}

    virtual AudioInfo GetInfo() const = 0;

    virtual uint Read(uint8* dest, uint size, double &time) = 0; // size in bytes

    virtual void JumpToTime(double time) = 0;
    virtual void Flush() = 0;
};

void InitAudioCapture();

void GetAudioDevices(Array<String> &into);

IAudioCapture *CreateAudioCaptureWASAPI(const CaptureConfig &config);