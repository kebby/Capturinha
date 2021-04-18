// TrainEngine.cpp : Defines the entry point for the application.
//

#include "types.h"
#include "math3d.h"
#include "system.h"
#include "graphics.h"

#include "screencapture.h"

#include <stdio.h>
#include <conio.h>

#include <string.h>

#include <stdarg.h>


int main(int argc, char** argv)
{       
    CaptureConfig config = 
    {
        .Filename = "c:\\temp\\capture",
    };

    //DbgOpenLog("c:\\temp\\capture.txt");
    auto capture = CreateScreenCapture(config);

    while (!_kbhit())
    {
        auto stats = capture->GetStats();
        printf("recd %5d frames, dupl %5d frames, %5.2f FPS, skew %6.2f ms // fs %d\r", stats.FramesCaptured + stats.FramesDuplicated, stats.FramesDuplicated, stats.FPS, stats.AVSkew*1000, IsFullscreen());
        Thread::Sleep(10);
    }
    _getch();

    delete capture;

    ExitD3D();

    //DbgCloseLog();
    return 0;
}
