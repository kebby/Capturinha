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

IEncode* encoder = nullptr;
IAudioCapture* audioCapture = nullptr;
Thread* processThread = nullptr;
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

    const uint audioSize = 768000;
    uint8* audioData = new uint8[audioSize];

    bool first = true;
    audioCapture->Flush();
    while (thread.IsRunning())
    {
        uint8* data;
        uint size;
        while (encoder->BeginGetPacket(data, size, 2))
        {
            output->SubmitVideoPacket(data, size);
            encoder->EndGetPacket();

            uint audio = audioCapture->Read(audioData, audioSize);
            if (audio)
                output->SubmitAudio(audioData, audio);

            first = false;
        }
    }

    delete output;
}

int main(int argc, char** argv)
{   
    InitD3D();

    audioCapture = CreateAudioCaptureWASAPI();
   
    printf("Go\n");
    bool first = true;
    int duplicated = 0;
    double lastFrameTime = GetTime();
    double frameDuration = 0;
    while (!_kbhit())
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
                    processThread = new Thread(ProcessThreadFunc);
                }
                else
                {
                    int todup = Max(0, (int)info.deltaFrames - duplicated - 1);
                    if (todup)
                    {
                        printf("dup2 %d\n", todup);
                        for (int i = 0; i < todup; i++)
                            encoder->DuplicateFrame();
                    }
                }
                encoder->SubmitFrame(info.tex);

                printf("got frame\n");

            }
            ReleaseFrame();

            // (it's that easy)
            duplicated = 0;
        }

        if (encoder && !first)
        {
            // if more than half a frame has passed without a new image, assume a skipped frame
            double time = GetTime();
            while (time - lastFrameTime > 1.5 * frameDuration)
            {
                printf("dup\n");
                encoder->DuplicateFrame();
                lastFrameTime += frameDuration;
                duplicated++;
            }
        }

       
    }
    _getch();

    if (encoder)
        encoder->Flush();
  
    delete processThread;
    delete encoder;
    delete audioCapture;

    ExitD3D();
    return 0;
}
