# Capturinha

A tool for real time screen and audio recording on Windows, using NVIDIA's NVENC and with an emphasis 
on performance, correctness (eg. frame rate stability) and configurability. Mostly made for demoscene productions
but you can use it with everything that's on your screen.

This is not a .kkapture successor. At least not yet. Mainly it's there to make the lives of demo party 
organizers - and other people who need a good video of their screen - easier.

### Why

Yes, there are a lot of screen capture tools around, built into Windows and GPU drivers even, 
but during testing I found them all lacking. There are either commercial solutions where the 
"Free" tier is so bad it's pretty much unusable and doesn't inspire you to shell out money 
for the real product, and the solutions built into Windows or the graphics driver lack configuration 
options such as choosing a constant quality encoding mode or even what codec to use - 
and all of them dropped frames like hot potatoes. So, having had some experience with graphics 
programming in general and with using NVENC and the FFmpeg libraries for encoding and muxing video 
and audio in particular, I thought "how hard can it be?"

### Building

##### Prerequisites
* Visual Studio 2019 with desktop/game C++ workloads installed (make sure to install ATL and Direct3D support). Older VS versions might work, too.
* FFmpeg 4.0 or later - http://ffmpeg.org/download.html or in binary form https://github.com/BtbN/FFmpeg-Builds/releases (you'll need the win64 shared LGPL build) - Copy the libavcodec, -format and -util, and swresample libs and DLLs into the project directory, as well as the respective contents of the include/ folder.
* NVIDIA CUDA Toolkit - https://developer.nvidia.com/cuda-downloads?target_os=Windows&target_arch=x86_64 
* NVIDIA Video Codec SDK 9 or later - https://developer.nvidia.com/nvidia-video-codec-sdk - copy nvEncodeAPI.h and nvencodeapi.lib into the project directory
* WTL 10.0 - included as NuGet package, please restore packages before build.

##### Build
* Press Ctrl-Shift-B, basically 

### Usage

To run Capturinha you'll need at least Windows 10 (64 bit) version 1903 or later, and an NVIDIA graphics card.
Your screen must be connected to that GPU, so sadly at the moment most laptops are out 😞

You'll be greeted by a configuration dialog. Select the screen you want to capture at the top, 
set up encoding and audio options in the middle, and choose an output directory, name and container format
at the bottom.

Now press "Start" (or press Win-F9). Recording will begin immediately, or as soon as a program goes into fullscreen
(and will pause when that program leaves fullscreen again). If the screen mode changes, the currently 
recording file will be closed and a new one will be opened.

Press "Stop" when you're done. That's basically it.

The current configuration gets stored in a file called `config.json` in the program directory.

##### Tips
* You can leave "only record when fullscreen" on and then just let Capturinha run minimized - 
  everything that goes into fullscreen will be recorded into its own file in the background
* If you experience audio/video drift, try using HDMI or DisplayPort audio. Those usually keep
  audio and video in sync (in contrast to GPUs and sound cards using their own clock each)

### TODO:

(This is the point where I state that I'd really like people to contribute :) )

(also, not listed by priority)

* Proper GPU capabilities and error handling. At the moment it just displays a message box and then bails.
* Backends for Non-NVIDIA encoders
  * AMD Video Code Engine
  * Intel Quick Sync
  * Media Foundation to get vendor independent
* Replace WTL with a more modern UI toolkit that doesn't look like a 90s revival party at 4AM when the lights go on
* Factor the encode and output parts into a library that can be used by other people that need a video writer in their engines/tools
* Refine full screen detection so it doesn't capture a few wrong frames at the end 
  when the app to record is already gone
* Optionally capture the mouse cursor (needs to be rendered into the target texture)
* Region of Interest or single window capture (only if it's in the foreground)
* As soon as it becomes relevant: HDR capture (using HEVC)
