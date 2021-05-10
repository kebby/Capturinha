### Capturinha change log

#### ??? 
* Compute shader based color space conversion
  * Less GPU and VRAM usage when encoding in 8 bit 4:2:0
  * Added 4:4:4 and 10 bits/channel profiles 
* Screen capture now supports 10/16 bits per channel modes
* Fixed file corruption when app window gets closed mid-recording

#### 0.1.1
* Removed AVI container (it can't do HEVC)
* Fixed occasional hang when recording stops
* Fixed HEVC muxing into Matroska container
* Fixed memory leak after recording

#### 0.1.0
* Initial release
