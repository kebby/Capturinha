#pragma once

#include "types.h"

struct CaptureConfig
{
    enum VideoCodec { H264, HEVC, };
    enum BitrateControl { CBR, CONSTQ, };
    enum Container { Avi, Mp4, Mov, Mkv };
    enum AudioCodec { PCM_S16, PCM_F32, MP3, AAC };

    struct VideoCodecConfig
    {
        VideoCodec Codec = VideoCodec::H264;
        // ... something something profile? (8 vs 10 bits, 4:4:4 vs 4:2:2, perf vs quality)
        
        BitrateControl UseBitrateControl = BitrateControl::CBR;
        int BitrateParameter = 10000000; // bit rate or qf
        int MaxBFrames = 1;
        int GopSize = 60; // 0: auto
    };

    // general
    String Filename;
    Container UseContainer = Container::Mp4;

    // video settings
    int OutputIndex = 0; // 0: default
    VideoCodecConfig CodecCfg;
    bool RecordOnlyFullscreen = true;

    // audio settings
    bool CaptureAudio = true;
    int AudioOutputIndex = 0; // 0: default
    AudioCodec UseAudioCodec = AudioCodec::AAC;
    int AudioBitrate = 320000; // not for PCM
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

// run a screen capture instance
IScreenCapture* CreateScreenCapture(const CaptureConfig &config);