### Capturinha change log

#### ???
* Oldschool Upscale: Scales up the screen in integer increments until a 
  certain target resolution is hit (or surpassed)

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
