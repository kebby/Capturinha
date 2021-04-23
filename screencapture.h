#pragma once

#include "types.h"

struct CaptureConfig
{
    enum class CodecProfile
    {
        H264_MAIN,
        H264_HIGH,
        H264_HIGH_444,
        H264_LOSSLESS,
        HEVC_MAIN,
        HEVC_MAIN10,
    };
    enum class BitrateControl { CBR, CONSTQP, };
    enum class Container { Avi, Mp4, Mov, Mkv };
    enum class AudioCodec { PCM_S16, PCM_F32, MP3, AAC };
    enum class FrameConfig { I, IP, /* IBP, IBBP, */ };

    struct VideoCodecConfig
    {
        CodecProfile Profile;
        // ... something something profile? (8 vs 10 bits, 4:4:4 vs 4:2:2, perf vs quality)

        BitrateControl UseBitrateControl = BitrateControl::CONSTQP;
        int BitrateParameter = 18; // bitrate in kbits/s or qp
        FrameConfig FrameCfg = FrameConfig::IP;
        int GopSize = 60; // 0: auto
    };

    // general
    String Filename;
    Container UseContainer = Container::Mp4;
    bool blinkScrollLock = true;

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
IScreenCapture* CreateScreenCapture(const CaptureConfig& config);