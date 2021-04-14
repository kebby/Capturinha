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
            .filename = "C:\\temp\\test.mov",
            .SizeX = sizeX,
            .SizeY = sizeY,
            .RateNum = rateNum,
            .RateDen = rateDen,
            .Audio = audioCapture->GetInfo(),
        };

        IOutput* output = CreateOutputLibAV(para);

        const uint audioSize = para.Audio.BytesPerSample*para.Audio.SampleRate;
        uint8* audioData = new uint8[audioSize];

        bool firstVideo = true;
        bool firstAudio = true;

        double firstVideoTime = 0;

        double vTimeSent = 0;
        double aTimeSent = 0;

        audioCapture->Flush();
        while (thread.IsRunning())
        {
            uint8* data;
            uint size;

            double videoTime;
            while (encoder->BeginGetPacket(data, size, 2, videoTime))
            {                    
                AVSkew += 0.01 * (aTimeSent - vTimeSent - AVSkew);

                output->SubmitVideoPacket(data, size);
                encoder->EndGetPacket();
                vTimeSent += (double)rateDen / rateNum;

                if (firstVideo)
                {
                    firstVideoTime = videoTime;
                    firstVideo = false;
                }

                double audioTime = 0;
                uint audio = audioCapture->Read(audioData, audioSize, audioTime);
                if (audio)
                {
                    if (firstAudio)
                    {
                        output->SetAudioDelay(audioTime - firstVideoTime);
                        aTimeSent += audioTime - firstVideoTime;
                        firstAudio = false;
                    }
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
        int duplicated = 0;
        double lastFrameTime = GetTime();
        double frameDuration = 0;
        while (thread.IsRunning())
        {
            CaptureInfo info;
            if (CaptureFrame(2, info))
            {
                lastFrameTime = GetTime();

                if (sizeX != info.sizeX || sizeY != info.sizeY || rateNum != info.rateNum || rateDen != info.rateDen)
                {
                    // (re)init encoder and processing thread, starts new output file
                    sizeX = info.sizeX;
                    sizeY = info.sizeY;
                    rateNum = info.rateNum;
                    rateDen = info.rateDen;
                    frameDuration = (double)info.rateDen / info.rateNum;

                    delete processThread;
                    processThread = nullptr;
                    delete encoder;

                    encoder = CreateEncodeNVENC();
                    encoder->Init(sizeX, sizeY, rateNum, rateDen);
                    first = true;
                }
                else
                {
                    // Encode frame
                    if (first)
                    {
                        first = false;
                        processThread = new Thread(Bind(this, &ScreenCapture::ProcessThreadFunc));
                    }
                    else
                    {
                        int todup = Max(0, (int)info.deltaFrames - duplicated - 1);
                        for (int i = 0; i < todup; i++)
                        {
                            encoder->DuplicateFrame();
                            AtomicInc(FramesDuplicated);
                        }

                        float curfps = (float)info.rateNum / (info.rateDen * info.deltaFrames);
                        if (!FPS) FPS = curfps;
                        FPS += 0.03 * (curfps - FPS);
                    }

                    encoder->SubmitFrame(info.tex, info.time);
                    AtomicInc(FramesCaptured);
                }
                ReleaseFrame();

                // (it's that easy)
                duplicated = 0;
            }

            if (encoder && !first)
            {
                // if more than a certain time has passed without a new image, assume a skipped frame
                double time = GetTime();
                while (time - lastFrameTime > 2.5 * frameDuration)
                {
                    encoder->DuplicateFrame();
                    AtomicInc(FramesDuplicated);
                    lastFrameTime += frameDuration;
                    duplicated++;
                    float curfps = (float)info.rateNum / (info.rateDen * duplicated);
                    FPS = FPS + 0.03 * (curfps - FPS);
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
    
    auto capture = new ScreenCapture();

    while (!_kbhit())
    {
        printf("recd %5d frames, dupl %5d frames, %5.2f FPS, skew %6.2f ms\r", capture->FramesCaptured, capture->FramesDuplicated, capture->FPS, capture->AVSkew*1000);
        Thread::Sleep(10);
    }
    _getch();

    delete capture;

    ExitD3D();
    return 0;
}
