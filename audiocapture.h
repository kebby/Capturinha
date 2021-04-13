#pragma once

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
};

class IAudioCapture
{
public:
    virtual ~IAudioCapture() {}

    virtual AudioInfo GetInfo() const = 0;

    virtual uint Read(uint8* dest, uint size) = 0; // size in bytes

    virtual void Flush() = 0;
};

IAudioCapture *CreateAudioCaptureWASAPI();