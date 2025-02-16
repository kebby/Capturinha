//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#pragma once

#include "types.h"
#include "json.h"

enum class CodecProfile
{
    H264_MAIN,
    H264_HIGH,
    H264_HIGH_444,
    HEVC_MAIN,
    HEVC_MAIN10,
    HEVC_MAIN_444,
    HEVC_MAIN10_444,
};

enum class BitrateControl { CBR, CONSTQP, };
enum class Container { Mp4, Mov, Mkv };
enum class AudioCodec { PCM_S16, PCM_F32, MP3, AAC };
enum class FrameConfig { I, IP, /* IBP, IBBP, */ };

JSON_DEFINE_ENUM(CodecProfile, "h264_main", "h264_high", "h264_high_444", "hevc_main", "hevc_main10", "hevc_main_444", "hevc_main10_444" )
JSON_DEFINE_ENUM(BitrateControl, "cbr", "constqp")
JSON_DEFINE_ENUM(Container, "mp4", "mov", "mkv")
JSON_DEFINE_ENUM(AudioCodec, "pcm_s16", "pcm_f32", "mp3", "aac")
JSON_DEFINE_ENUM(FrameConfig, "i", "ip" )

struct VideoCodecConfig
{
    CodecProfile Profile = CodecProfile::H264_MAIN;

    BitrateControl UseBitrateControl = BitrateControl::CONSTQP;
    uint BitrateParameter = 24; // bitrate in kbits/s or qp

    FrameConfig FrameCfg = FrameConfig::IP;
    uint GopSize = 60; // 0: auto

    JSON_BEGIN();
        JSON_ENUM(Profile);
        JSON_ENUM(UseBitrateControl);
        JSON_VALUE(BitrateParameter);
        JSON_ENUM(FrameCfg);
        JSON_VALUE(GopSize);
    JSON_END();
};

struct CaptureConfig
{   
    // general
    String Directory;
    String NamePrefix = "capture";
    Container UseContainer = Container::Mov;
    bool BlinkScrollLock = true;

    // video settings
    uint OutputIndex = 0; // 0: default
    bool Upscale = false;
    uint UpscaleTo = 2160;
    VideoCodecConfig CodecCfg;
    bool RecordOnlyFullscreen = true;

    // audio settings
    bool CaptureAudio = true;
    uint AudioOutputIndex = 0; // 0: default
    AudioCodec UseAudioCodec = AudioCodec::PCM_S16;
    uint AudioBitrate = 320; // not for PCM

    JSON_BEGIN()
        JSON_VALUE(Directory)
        JSON_VALUE(NamePrefix)
        JSON_ENUM(UseContainer)
        JSON_VALUE(BlinkScrollLock)
        JSON_VALUE(OutputIndex)
        JSON_VALUE(Upscale)
        JSON_VALUE(UpscaleTo)
        JSON_VALUE(CodecCfg)
        JSON_VALUE(RecordOnlyFullscreen)
        JSON_VALUE(CaptureAudio)
        JSON_VALUE(AudioOutputIndex)
        JSON_ENUM(UseAudioCodec)
        JSON_VALUE(AudioBitrate)
    JSON_END();
};


struct CaptureStats
{
    enum class CaptureFormat { Unknown, P8, P10, P16, P16F };

    struct Frame
    {
        double FPS;
        double AVSkew;
        double Bitrate;
    };

    bool Recording;

    int SizeX;
    int SizeY;
    CaptureFormat Fmt;
    bool HDR;
    double Time;
    double FPS;
    double AvgBitrate;
    double MaxBitrate;
    Array<Frame> Frames = Array<Frame>((size_t)40000);

    uint FramesCaptured;
    uint FramesDuplicated;      

    float VU[32] = { -1.f };
    float VUPeak[32] = { -1.f };

    String Filename;
};


class IScreenCapture
{
public:
    virtual ~IScreenCapture() {}

    virtual const CaptureStats &GetStats() = 0;
};

// run a screen capture instance
IScreenCapture* CreateScreenCapture(const CaptureConfig& config);