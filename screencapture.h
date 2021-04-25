#pragma once

#include "types.h"
#include "json.h"

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


JSON_DEFINE_ENUM(CodecProfile,   ("h264_main", "h264_high", "h264_high_444", "h264_lossless", "hevc_main", "hevc_main10" ))
JSON_DEFINE_ENUM(BitrateControl, ("cbr", "constqp"))
JSON_DEFINE_ENUM(Container,      ("avi", "mp4", "mov", "mkv"))
JSON_DEFINE_ENUM(AudioCodec,     ("pcm_s16", "pcm_f32", "mp3", "aac"))
JSON_DEFINE_ENUM(FrameConfig,    ("i", "ip"/* ,"ibp", "ibbp", */))


struct VideoCodecConfig
{
    CodecProfile Profile = CodecProfile::H264_MAIN;
    // ... something something profile? (8 vs 10 bits, 4:4:4 vs 4:2:2, perf vs quality)

    BitrateControl UseBitrateControl = BitrateControl::CONSTQP;
    uint BitrateParameter = 18; // bitrate in kbits/s or qp
    FrameConfig FrameCfg = FrameConfig::IP;
    uint GopSize = 60; // 0: auto

    JSON_BEGIN(VideoCodecConfig);
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
    String NamePrefix;
    Container UseContainer = Container::Mp4;
    bool BlinkScrollLock = true;

    // video settings
    uint OutputIndex = 0; // 0: default
    VideoCodecConfig CodecCfg;
    bool RecordOnlyFullscreen = true;

    // audio settings
    bool CaptureAudio = true;
    uint AudioOutputIndex = 0; // 0: default
    AudioCodec UseAudioCodec = AudioCodec::AAC;
    uint AudioBitrate = 320; // not for PCM

    JSON_BEGIN(VideoCodecConfig)
        JSON_VALUE(Directory)
        JSON_VALUE(NamePrefix)
        JSON_ENUM(UseContainer)
        JSON_VALUE(BlinkScrollLock)
        JSON_VALUE(OutputIndex)
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