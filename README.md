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
* NVIDIA Video Codec SDK 11 or later - https://developer.nvidia.com/nvidia-video-codec-sdk - copy nvEncodeAPI.h and nvencodeapi.lib into the project directory
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

##### About the settings

The codecs currently supported by NVENC are h.264 and HEVC (also known as h.265). If your graphics card isn't too old 
(older than let's say the GTX 900 series), HEVC is usually better in quality (per bitrate) _as well as encoder
performance_, so if you haven't got a good reason to use h.264 (for example you want to encode for a device
that only supports that), choose HEVC.

The 4:4:4 and Main10 profiles are choices for high quality encoding - 4:4:4 means no chroma subsampling and 
will result in a less washed out image when there's a lot of colorful high resolution detail, and the Main10 
profiles will encode in 10 bits per color channel instead of 8. This even helps for 8 bit screens, as there 
is some slight loss of precision when converting from the screen's RGB into the YCbCr color space that the encoded video 
is stored in, so choosing 10 bits will reduce some banding artifacts. But beware that if you capture for uploading 
to a video streaming site, as far as I know none of them support these high quality settings, and the added
fidelity will be lost (the files will upload and play just fine though) - so encoding in the "normal" profiles is good enough for that. 
Also you'll need at least a GTX 1060 to unlock these modes for HEVC.

The "Const QP" rate control mode encodes in constant quality while wildly varying the bitrate according to how much is happening on screen. 
The values go from 1 (best) to 52 (worst), and a value of 24 for h.264 and 28 for HEVC is already really good. Use this mode when you
don't want to put a file directly onto the internet or play on limited/mobile devices - for presentation from a sufficiently
powered machine, editing, archival, and uploading to video streaming sites (that transcode all incoming files anyway)
 Const QP is vastly preferred.

The frame layout option should be kept at "I+P" unless you want to radically edit the resulting file - "I", aka "I frames only", 
forgoes any "let's predict what the next frame could look like" magic amdstores each frame as complete image. Perfect for editing but absolutely devastating for bitrate or file size. The GOP length determines how
many P frames (as in "predicted") go between two I frames, aka how many frames go by until the decoder resets to a known state. Increasing this
value helps with bitrate a lot but it makes it harder to edit or even seek in the files, and you might experience that the image
drifts/washes away and then "comes back" every so and so seconds. When in doubt, leave at the default.

(No, B frames are currently not supported, simply because they don't help too much with the use cases Capturinha has, and can make playback worse)

##### Tips
* The MP4 container can't contain PCM audio, so trying this combination will result in an error.
* You can leave "only record when fullscreen" on and then just let Capturinha run minimized - 
  everything that goes into fullscreen will be recorded into its own file in the background.
* If you experience audio/video drift, try using HDMI or DisplayPort audio. Those usually keep
  audio and video in sync (in contrast to GPUs and sound cards using their own clock each).
* There seems to be an issue with 10bit screens and certain 8bit fullscreen modes regarding the
  gamma Windows reports, so if the recording comes out way too dark, setting the screen to 
  8 bits per pixel should fix it.

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
