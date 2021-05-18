//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#include <math.h>

#include "types.h"
#include "system.h"
#include "graphics.h"

#include "audiocapture.h"
#include "colormath.h"
#include "encode.h"
#include "output.h"

#include "ScreenCapture.h"

#include "resource.h"

class ScreenCapture : public IScreenCapture
{
    CaptureConfig Config;

    IEncode* encoder = nullptr;
    IAudioCapture* audioCapture = nullptr;
    AudioInfo audioInfo = {};
    Thread* processThread = nullptr;
    Thread* captureThread = nullptr;
    uint sizeX = 0, sizeY = 0, rateNum = 0, rateDen = 0;
    uint bufSizeX = 0, bufSizeY = 0;

    CaptureStats Stats = {};
    double avSkew = 0;
    double fps = 0;
    double bitrate = 0;

    void CalcVU(const uint8 *ptr, uint size)
    {
        uint ch = audioInfo.Channels;
        if (audioInfo.Format != AudioFormat::F32)
            ch = 0;

        size /= 4 * ch;

        for (uint i = 0; i < ch; i++)
        {
            const float* data = ((float*)ptr) + i;
            float cvu = Stats.VU[i];
            for (uint s = 0; s < size; s++)
            {
                float v = fabsf(*data);
                if (v > cvu)
                    cvu = v;
                else
                    cvu *= 0.9999f;
                data += ch;
            }
            Stats.VU[i] = cvu;
            Stats.VUPeak[i] = Max(Stats.VUPeak[i], cvu);
        }

        for (int i = ch; i < 32; i++)
            Stats.VU[i] = -1;        
    }

    void ProcessThreadFunc(Thread& thread)
    {
        static const char* const extensions[] = { "mp4", "mov", "mkv" };

        String prefix = Config.Directory + "\\" + Config.NamePrefix;

        auto systime = GetSystemTime();
        auto filename = String::PrintF("%s_%04d-%02d-%02d_%02d.%02d.%02d_%dx%d_%.4gfps.%s",
            (const char*)prefix,
            systime.year, systime.month, systime.day, systime.hour, systime.minute, systime.second,
            sizeX, sizeY, (double)rateNum / rateDen,
            extensions[(int)Config.UseContainer]
        );

        audioInfo = audioCapture ? audioCapture->GetInfo() : AudioInfo{ .Format = AudioFormat::None };

        OutputPara para =
        {
            .filename = filename,
            .SizeX = sizeX,
            .SizeY = sizeY,
            .RateNum = rateNum,
            .RateDen = rateDen,
            .Audio = audioInfo,
            .CConfig = &Config,
        };

        Stats = {};
        Stats.Filename = filename;
        Stats.FPS = (double)rateNum / rateDen;
        Stats.SizeX = sizeX;
        Stats.SizeY = sizeY;
        
        IOutput* output = CreateOutputLibAV(para);

        const uint audioSize = para.Audio.BytesPerSample * (para.Audio.SampleRate / 10);
        uint8* audioData = new uint8[audioSize];

        bool firstVideo = true;
        bool firstAudio = true;

        double firstVideoTime = 0;
        
        double vTimeSent = 0;
        double aTimeSent = 0;
        bool scrlOn = true;
        if (Config.BlinkScrollLock)
            SetScrollLock(true);

        int frameCount = 0;
        uint totalBytes = 0;

        while (thread.IsRunning())
        {
            uint8* data;
            uint size;

            double videoTime;
            while (encoder->BeginGetPacket(data, size, 2, videoTime))
            {
                output->SubmitVideoPacket(data, size);
                encoder->EndGetPacket();
                vTimeSent += (double)rateDen / rateNum;

                if (firstVideo)
                {
                    firstVideoTime = videoTime;
                    firstVideo = false;
                    if (audioCapture)
                        audioCapture->JumpToTime(firstVideoTime);
                }

                if (audioCapture)
                {
                    double audioTime = 0;
                    uint audio = audioCapture->Read(audioData, audioSize, audioTime);
                    if (audio)
                    {
                        output->SubmitAudio(audioData, audio);
                        aTimeSent += (double)audio / ((double)para.Audio.BytesPerSample * para.Audio.SampleRate);
                        CalcVU(audioData, audio);
                    }
                    avSkew += 0.03 * (aTimeSent - vTimeSent - avSkew);
                }

                if (Config.BlinkScrollLock)
                {
                    bool blink = fmod(GetTime(), 1) < 0.5f;
                    if (blink != scrlOn)
                    {
                        SetScrollLock(blink);
                        scrlOn = blink;
                    }
                }

                frameCount++;
                totalBytes += size;

                double br = (8. * size * rateNum) / (1000. * rateDen);
                bitrate += 0.03 * (br  - bitrate);
                Stats.AvgBitrate = (8. * (double)totalBytes * rateNum) / (1000. * frameCount * rateDen);
                Stats.MaxBitrate = Max(Stats.MaxBitrate, bitrate);
                Stats.Time = (double)frameCount * rateDen / rateNum;
                Stats.Frames.PushTail(CaptureStats::Frame{ .FPS = fps, .AVSkew = avSkew, .Bitrate = bitrate });
            }        
        }

        if (Config.BlinkScrollLock && scrlOn)
            SetScrollLock(false);

        delete output;
    }


    struct CbConvert
    {
        Mat44 colormatrix;    // needs to have bpp baked in (so eg. *255)
        uint pitch;           // bytes per line
        uint height;          // # of lines
        uint scale;           // upscale factor, only when UPSCALE is defined
        uint _pad0;
        uint offsetX;         // pixel offset into input texture
        uint offsetY;
        uint _pad1[2];
    };

    void CaptureThreadFunc(Thread& thread)
    {
        bool first = true;
        int duplicated = 0;
        int over = 0;
        double lastFrameTime = GetTime();
        double ltf2 = lastFrameTime;
        double frameDuration = 0;
        uint upscale = 1;

        double vInSkew = 0;
        uint64 lastFrameCount = 0;

        Mat44 colorMatrix;
        RCPtr<GpuByteBuffer> outBuffer;

        uint scrSizeX = 0, scrSizeY = 0;

        while (thread.IsRunning())
        {
            bool record = !Config.RecordOnlyFullscreen || IsFullscreen();
            Stats.Recording = record;

            CaptureInfo info;
            if (CaptureWindow(2, 0, info))
            {
                double time = GetTime();
                double deltaf = (time - ltf2) * (double)info.rateNum / info.rateDen;

                lastFrameTime = ltf2 = time;

                if (!record)
                {
                    info.tex.Clear();
                    Delete(processThread);
                    Delete(encoder);
                    scrSizeX = scrSizeY = 0;
                    ReleaseFrame();
                    for (int i = 0; i < 32; i++)
                        if (Stats.VU[i] > 0)
                            Stats.VU[i] = 0;
                    continue;
                }

                if (scrSizeX != info.sizeX || scrSizeY != info.sizeY || rateNum != info.rateNum || rateDen != info.rateDen)
                {
                    // (re)init encoder and processing thread, starts new output file
                    scrSizeX = sizeX = info.sizeX;
                    scrSizeY = sizeY = info.sizeY;
                    rateNum = info.rateNum;
                    rateDen = info.rateDen;
                    frameDuration = (double)info.rateDen / info.rateNum;

                    upscale = 1;
                    if (Config.Upscale)
                    {
                        while (sizeY * upscale < Config.UpscaleTo)
                            upscale++;
                        sizeX *= upscale;
                        sizeY *= upscale;
                    }

                    if (encoder)
                        encoder->Flush();

                    Delete(processThread);
                    Delete(encoder);

                    //                  DPrintF("\n\n*************************** NEW\n\n\n");
                    encoder = CreateEncodeNVENC(Config);

                    bufSizeX = Align(sizeX, 8u);
                    bufSizeY = Align(sizeY, 8u);
                    auto fmt = encoder->GetBufferFormat();
                    auto fi = GetFormatInfo(fmt, bufSizeX, bufSizeY);
                    outBuffer = new GpuByteBuffer(fi.lines * fi.pitch, GpuBuffer::Usage::GpuOnly);
                   
                    auto source = LoadResource(IDR_COLORCONVERT, TEXTFILE);
                    Array<ShaderDefine> defines =
                    {
                        ShaderDefine { "OUTFORMAT", String::PrintF("%d", (int)fmt) },
                        ShaderDefine { "UPSCALE", upscale > 1 ? "1":"0" },
                    };
                    Shader = CompileShader(Shader::Type::Compute, source, "csc", defines, "colorconvert.hlsl");

                    switch (fmt)
                    {
                    case IEncode::BufferFormat::BGRA8: 
                        colorMatrix = {}; 
                        break;
                    default:
                        colorMatrix = MakeRGB2YUV44(Rec709, fi.ymin, fi.ymax, fi.uvmin, fi.uvmax);
                    }
                    colorMatrix = colorMatrix * Mat44::Scale(fi.amp);
                    
                    encoder->Init(sizeX, sizeY, bufSizeX, bufSizeY, rateNum, rateDen, outBuffer);
                    first = true;
                    duplicated = 0;
                    over = 0;

                    lastFrameCount = 0;
                }
                else
                {
                    int deltaFrames = (int)(info.frameCount - lastFrameCount);
                    ASSERT(deltaFrames < 0x8000000000000000);
                    lastFrameCount = info.frameCount;

                    // Encode frame
                    if (first)
                    {
                        first = false;
                        processThread = new Thread(Bind(this, &ScreenCapture::ProcessThreadFunc));
                    }
                    else
                    {
                        int dup = Max(1, deltaFrames) - 1 - duplicated;

                        if (dup < 0)
                        {
                            //DPrintF("%6.2f: OVER %d\n", time, -dup);
                            over -= dup;
                            dup = 0;
                        }
                        else
                        {
                            int doover = Min(dup, over);
                            dup -= doover;
                            over -= doover;
                        }

                        for (int i = 0; i < dup; i++)
                        {
                            //DPrintF("%6.2f: dup1\n", time);
                            encoder->DuplicateFrame();
                            AtomicInc(Stats.FramesDuplicated);
                        }

                        if (deltaFrames)
                        {
                            double curfps = (double)info.rateNum / ((double)info.rateDen * deltaFrames);
                            if (!fps) fps = curfps;
                            fps += 0.03 * (curfps - fps);
                        }
                    }

                    //DPrintF("%6.2f: submit\n", time);
                    if (deltaFrames)
                    {
                        auto fi = GetFormatInfo(encoder->GetBufferFormat(), bufSizeX, bufSizeY);

                        // color space conversion
                        CBuffer<CbConvert> cb;
                        cb->colormatrix = colorMatrix.Transpose();
                        cb->pitch = fi.pitch;
                        cb->height = bufSizeY;
                        cb->scale = upscale;
                        cb->offsetX = info.offsX;
                        cb->offsetY = info.offsY;

                        CBindings bind;
                        bind.res[0] = info.tex;
                        bind.uav[0] = outBuffer;
                        bind.cb[0] = &cb;

                        Dispatch(Shader, bind, (sizeX + 7) / 8, (sizeY + 7) / 8, 1);

                        encoder->SubmitFrame(info.time);
                        AtomicInc(Stats.FramesCaptured);
                    }
                }
                info.tex.Clear();
                ReleaseFrame();

                // (it's that easy)
                duplicated = 0;
                //DPrintF("%6.2f (%4.2f): got %d, dup %d, over %d, avskew %5.2fms, vskew %5.2ff\n", time, deltaf, info.frameCount, duplicated, over, 1000. * Stats.AVSkew, vInSkew);

            }

            if (encoder && !first)
            {
                // if more than a certain time has passed without a new image, assume a skipped frame
                double time = GetTime();
                while (time - lastFrameTime > 2.5 * frameDuration)
                {
                    if (over)
                    {
                        //DPrintF("%6.2f: comp\n", time);
                        over--;
                    }
                    else
                    {
                        //DPrintF("%6.2f: dup2\n", time);
                        encoder->DuplicateFrame();
                        AtomicInc(Stats.FramesDuplicated);
                        duplicated++;
                    }

                    lastFrameTime += frameDuration;
                    double curfps = (double)info.rateNum / ((double)info.rateDen * (duplicated + 1.0));
                    fps += 0.03 * (curfps - fps);
                }
            }
        }

        if (encoder)
            encoder->Flush();

        delete processThread;
        delete encoder;
       
    }

public:

    RCPtr<Shader> Shader;

 
    ScreenCapture(const CaptureConfig& cfg) : Config(cfg)
    {
        InitD3D(Config.OutputIndex);
       
        if (Config.CaptureAudio)
            audioCapture = CreateAudioCaptureWASAPI(Config);
        captureThread = new Thread(Bind(this, &ScreenCapture::CaptureThreadFunc));

        for (int i = 0; i < 32; i++)
            Stats.VU[i] = i ? -1.0f : 0.0f;
    }

    ~ScreenCapture()
    {
        delete captureThread;
        delete audioCapture;
        ExitD3D();
    }

    const CaptureStats &GetStats() override { return Stats; }
};


IScreenCapture* CreateScreenCapture(const CaptureConfig& config) { return new ScreenCapture(config); }