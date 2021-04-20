#pragma once

#include "types.h"

struct CaptureConfig
{
    enum class VideoCodec { H264, HEVC, };
    enum class BitrateControl { CBR, CONSTQP, };
    enum class Container { Avi, Mp4, Mov, Mkv };
    enum class AudioCodec { PCM_S16, PCM_F32, MP3, AAC };
    enum class FrameConfig { I, IP, IBP, IBBP,};

    struct VideoCodecConfig
    {
        VideoCodec Codec = VideoCodec::H264;
        // ... something something profile? (8 vs 10 bits, 4:4:4 vs 4:2:2, perf vs quality)
        
        BitrateControl UseBitrateControl = BitrateControl::CONSTQP;
        int BitrateParameter = 18; // bit rate or qf
        FrameConfig FrameCfg = FrameConfig::IP;
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