### Capturinha change log

### 0.5.0: Maintenance release 
* Dependencies are now handled by vcpkg
* Updated to ffmpeg 7
* Use ffnvcodec package instead of NVIDIA Video/CUDA SDKs
* Query the proper monitor name for the screen dropdown

### 0.4.1: Maintenance release 
* Show recorded pixel format in statistics view
* Fixed wrong video format in MKV EBML chunk when recording HDR
* Better HDR detection and passthrough for 10bit outputs that are already in Rec.2100 format

#### 0.4.0: High Dynamic Range
* Added HDR capture - if the screen is set to HDR we will now faithfully 
  encode it in full Rec.2100 glory (HEVC 10 bit profiles only)
* And while we're at it, mark SDR captures as having the sRGB transfer 
  function (instead of Rec.709) which should improve shadows for players that care

#### 0.3.0: Scale up those pixels!
* Oldschool Upscale: Scales up the screen in integer increments until a 
  certain target resolution is hit (or surpassed)
* More memory management fixes
* Fixed Direct3D error on some machines
* Error box shows full error text (instead of just a code) now also in release builds

#### 0.2.0: The Quality Update
* Added Compute Shader based color space conversion
  * Less GPU and VRAM usage when encoding in 8 bit 4:2:0
  * Added 4:4:4 and 10 bits/channel profiles 
* Screen capture now supports 10/16 bits per channel modes
* Tested a lot and tweaked encoder parameters for optimum performance/quality
* Added Win-F9 hotkey to start/stop recording
* Added a bit of help re. encoder settings to the README
* Fixed file corruption when app window gets closed mid-recording
* Scroll Lock light doesn't get stuck on "on" after recording anymore :)

#### 0.1.1: Bugfix release
* Removed AVI container (it can't do HEVC)
* Fixed occasional hang when recording stops
* Fixed HEVC muxing into Matroska container
* Fixed memory leak after recording

#### 0.1.0
* Initial release
