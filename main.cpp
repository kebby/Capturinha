// TrainEngine.cpp : Defines the entry point for the application.
//

#include "types.h"
#include "math3d.h"
#include "system.h"
#include "graphics.h"

#include "encode.h"
#include "output.h"
#include "audiocapture.h"

#include <stdio.h>
#include <conio.h>

class ScreenCapture
{
    IEncode* encoder = nullptr;
    IAudioCapture* audioCapture = nullptr;
    Thread* processThread = nullptr;
    Thread* captureThread = nullptr;
    uint sizeX = 0, sizeY = 0, rateNum = 0, rateDen = 0;

    void ProcessThreadFunc(Thread& thread)
    {
        OutputPara para =
        {
            .filename = "C:\\temp\\capture",
            .SizeX = sizeX,
            .SizeY = sizeY,
            .RateNum = rateNum,
            .RateDen = rateDen,
            .Audio = audioCapture->GetInfo(),
        };

        IOutput* output = CreateOutputLibAV(para);

        const uint audioSize = para.Audio.BytesPerSample*(para.Audio.SampleRate/10);
        uint8* audioData = new uint8[audioSize];

        bool firstVideo = true;
        bool firstAudio = true;

        double firstVideoTime = 0;

        double vTimeSent = 0;
        double aTimeSent = 0;

        while (thread.IsRunning())
        {
            uint8* data;
            uint size;

            double videoTime;
            while (encoder->BeginGetPacket(data, size, 2, videoTime))
            {                    
                AVSkew += 0.03 * (aTimeSent - vTimeSent - AVSkew);

                output->SubmitVideoPacket(data, size);
                encoder->EndGetPacket();
                vTimeSent += (double)rateDen / rateNum;

                if (firstVideo)
                {
                    firstVideoTime = videoTime;
                    firstVideo = false;
                    audioCapture->JumpToTime(firstVideoTime);
                }

                double audioTime = 0;
                uint audio = audioCapture->Read(audioData, audioSize, audioTime);
                if (audio)
                {
                    output->SubmitAudio(audioData, audio);
                    aTimeSent += (double)audio / (para.Audio.BytesPerSample * para.Audio.SampleRate);
                }
            }

        }

        delete output;
    }

public:

    uint FramesCaptured = 0;
    uint FramesDuplicated = 0;
    float FPS = 0;
    double AVSkew = 0;
 
    int invsout = 0;

    ScreenCapture()
    {
        InitD3D();
        audioCapture = CreateAudioCaptureWASAPI();
        captureThread = new Thread(Bind(this, &ScreenCapture::CaptureThreadFunc));
    }

    ~ScreenCapture()
    {
        delete captureThread;
        delete audioCapture;
        ExitD3D();
    }

    void CaptureThreadFunc(Thread &thread)
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
            CaptureInfo info;
            if (CaptureFrame(2, info))
            {
                double time = GetTime();
                double deltaf = (time - ltf2) * (double)info.rateNum / info.rateDen;

                lastFrameTime = ltf2 = time;
                 
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

                    delete processThread;
                    processThread = nullptr;
                    delete encoder;

                    DPrintF("\n\n*************************** NEW\n\n\n");

                    encoder = CreateEncodeNVENC();
                    encoder->Init(sizeX, sizeY, rateNum, rateDen);
                    first = true;
                    duplicated = 0;
                    over = 0;
                    preroll = 3;

                    FramesCaptured = 0;
                    FramesDuplicated = 0;
                    FPS = 0;
                    AVSkew = 0;
                    invsout = 0;
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
                        invsout += deltaFrames;
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
                            AtomicInc(FramesDuplicated);
                        }

                        if (deltaFrames)
                        {
                            float curfps = (float)info.rateNum / (info.rateDen * deltaFrames);
                            if (!FPS) FPS = curfps;
                            FPS += 0.03f * (curfps - FPS);
                        }
                    }

                    //DPrintF("%6.2f: submit\n", time);
                    if (deltaFrames)
                    {
                        if (!preroll)
                        {
                            encoder->SubmitFrame(info.tex, info.time);
                            AtomicInc(FramesCaptured);
                        }
                        else
                            preroll--;
                    }
                }
                ReleaseFrame();

                // (it's that easy)
                duplicated = 0;
                DPrintF("%6.2f (%4.2f): got %d, dup %d, over %d, avskew %5.2fms, vskew %5.2ff\n", time, deltaf, info.frameCount, duplicated, over, 1000. * AVSkew, vInSkew);

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
                        AtomicInc(FramesDuplicated);
                        duplicated++;
                    }

                    lastFrameTime += frameDuration;
                    float curfps = (float)info.rateNum / (info.rateDen * (duplicated+1));
                    FPS = FPS + 0.03f * (curfps - FPS);
                }
            }
        }

        if (encoder)
            encoder->Flush();

        delete processThread;
        delete encoder;
        
    }
};


int main(int argc, char** argv)
{   
    
    //DbgOpenLog("c:\\temp\\capture.txt");

    auto capture = new ScreenCapture();

    while (!_kbhit())
    {
        printf("recd %5d frames, dupl %5d frames, %5.2f FPS, skew %6.2f ms\r", capture->FramesCaptured + capture->FramesDuplicated, capture->FramesDuplicated, capture->FPS, capture->AVSkew*1000);
        Thread::Sleep(10);
    }
    _getch();

    delete capture;

    ExitD3D();
    //DbgCloseLog();
    return 0;
}
