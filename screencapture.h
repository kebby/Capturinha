#pragma once



struct CaptureConfig
{
    enum Codec { H264, HEVC, };




    int GpuIndex;
    int OutputIndex;

    Codec UseCodec;

    int AudioOutputIndex;

    bool RecordOnlyFullscreen = true;
};

struct CaptureStats
{
    uint FramesCaptured;
    uint FramesDuplicated;
    double FPS;
    double AVSkew;
};

class IScreenCapture
{
public:
    virtual ~IScreenCapture() {}

    virtual CaptureStats GetStats() = 0;
};

IScreenCapture* CreateScreenCapture(const CaptureConfig &config);