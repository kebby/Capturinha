#include <math.h>

#include "types.h"
#include "system.h"
#include "graphics.h"

#include "audiocapture.h"
#include "encode.h"
#include "output.h"

#include "ScreenCapture.h"


class ScreenCapture : public IScreenCapture
{
    CaptureConfig Config;

    IEncode* encoder = nullptr;
    IAudioCapture* audioCapture = nullptr;
    Thread* processThread = nullptr;
    Thread* captureThread = nullptr;
    uint sizeX = 0, sizeY = 0, rateNum = 0, rateDen = 0;

    CaptureStats Stats;

    void ProcessThreadFunc(Thread& thread)
    {

        static const char* const extensions[] = { "avi", "mp4", "mov", "mkv" };

        auto systime = GetSystemTime();
        auto filename = String::PrintF("%s_%04d-%02d-%02d_%02d.%02d.%02d_%dx%d_%.4gfps.%s",
            (const char*)Config.Filename,
            systime.year, systime.month, systime.day, systime.hour, systime.minute, systime.second,
            sizeX, sizeY, (double)rateNum / rateDen,
            extensions[(int)Config.UseContainer]
        );

        OutputPara para =
        {
            .filename = filename,
            .SizeX = sizeX,
            .SizeY = sizeY,
            .RateNum = rateNum,
            .RateDen = rateDen,
            .Audio = audioCapture ? audioCapture->GetInfo() : AudioInfo {.Format = AudioFormat::None },
            .CConfig = &Config,
        };

        IOutput* output = CreateOutputLibAV(para);

        const uint audioSize = para.Audio.BytesPerSample * (para.Audio.SampleRate / 10);
        uint8* audioData = new uint8[audioSize];

        bool firstVideo = true;
        bool firstAudio = true;

        double firstVideoTime = 0;

        double vTimeSent = 0;
        double aTimeSent = 0;
        bool scrlOn = true;
        if (Config.blinkScrollLock)
            SetScrollLock(true);

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
                        aTimeSent += (double)audio / (para.Audio.BytesPerSample * para.Audio.SampleRate);
                    }
                    Stats.AVSkew += 0.03 * (aTimeSent - vTimeSent - Stats.AVSkew);
                }

                if (Config.blinkScrollLock)
                {
                    bool blink = fmod(2 * GetTime(), 1) < 0.5f;
                    if (blink != scrlOn)
                    {
                        SetScrollLock(blink < 0.5f);
                        scrlOn = blink;
                    }
                }
            }        
        }

        if (Config.blinkScrollLock && scrlOn)
            SetScrollLock(false);

        delete output;
    }

    void CaptureThreadFunc(Thread& thread)
    {
        bool first = true;
        int preroll = 3;
        int duplicated = 0;
        int over = 0;
        double lastFrameTime = GetTime();
        double ltf2 = lastFrameTime;
        double frameDuration = 0;

        double vInSkew = 0;
        uint64 lastFrameCount = 0;

        while (thread.IsRunning())
        {
            bool record = !Config.RecordOnlyFullscreen || IsFullscreen();

            CaptureInfo info;
            if (CaptureFrame(2, info))
            {
                double time = GetTime();
                double deltaf = (time - ltf2) * (double)info.rateNum / info.rateDen;

                lastFrameTime = ltf2 = time;

                if (!record)
                {
                    Delete(processThread);
                    Delete(encoder);
                    sizeX = sizeY = 0;
                    ReleaseFrame();
                    continue;
                }

                if (sizeX != info.sizeX || sizeY != info.sizeY || rateNum != info.rateNum || rateDen != info.rateDen)
                {
                    // (re)init encoder and processing thread, starts new output file
                    sizeX = info.sizeX;
                    sizeY = info.sizeY;
                    rateNum = info.rateNum;
                    rateDen = info.rateDen;
                    frameDuration = (double)info.rateDen / info.rateNum;

                    if (encoder)
                        encoder->Flush();

                    Delete(processThread);
                    Delete(encoder);

//                  DPrintF("\n\n*************************** NEW\n\n\n");

                    
                    encoder = CreateEncodeNVENC(Config);
                    encoder->Init(sizeX, sizeY, rateNum, rateDen);
                    first = true;
                    duplicated = 0;
                    over = 0;
                    preroll = 3;

                    Stats = {};
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
                            double curfps = (double)info.rateNum / (info.rateDen * deltaFrames);
                            if (!Stats.FPS) Stats.FPS = curfps;
                            Stats.FPS += 0.03 * (curfps - Stats.FPS);
                        }
                    }

                    //DPrintF("%6.2f: submit\n", time);
                    if (deltaFrames)
                    {
                        if (!preroll)
                        {
                            encoder->SubmitFrame(info.tex, info.time);
                            AtomicInc(Stats.FramesCaptured);
                        }
                        else
                            preroll--;
                    }
                }
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
                    double curfps = (double)info.rateNum / (info.rateDen * (duplicated + 1));
                    Stats.FPS += 0.03 * (curfps - Stats.FPS);
                }
            }
        }

        if (encoder)
            encoder->Flush();

        delete processThread;
        delete encoder;

    }

public:

    ScreenCapture(const CaptureConfig& cfg) : Config(cfg)
    {
        InitD3D(Config.OutputIndex);
        if (Config.CaptureAudio)
            audioCapture = CreateAudioCaptureWASAPI(Config);
        captureThread = new Thread(Bind(this, &ScreenCapture::CaptureThreadFunc));
    }

    ~ScreenCapture()
    {
        delete captureThread;
        delete audioCapture;
        ExitD3D();
    }

    CaptureStats GetStats() override { return Stats; }
};


IScreenCapture* CreateScreenCapture(const CaptureConfig& config) { return new ScreenCapture(config); }