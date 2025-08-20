# esp32_sampler_playground
test project for sampler functionality on an esp32

needs good powermanagement to run properly otherwise expect clicks and pops. Runnning an external power works well, but I am finding that running it from the 5vo on the esp into vin also gives good quality

if running on external power then you have to be really careful to manage the usb power vs external power to not fry something. 

Current Limitations with ESP8266Audio Library

The ESP8266Audio library you're currently using has fundamental limitations for polyphonic playback - it wasn't designed to play multiple WAV files simultaneously Play two wav files simultaneously? · Issue #24 · earlephilhower/ESP8266Audio, which explains why you can play a backing track (MP3) and one WAV file but not multiple WAV samples at once.

## ESP32 Audio Library Comparison

### ESP8266Audio
Strengths:

Excellent simultaneous audio mixing (MP3 + WAV)
High audio quality with no degradation
Stable, production-ready
Low latency for real-time effects
Optimized for ESP32 dual-core architecture
Mature codebase with proven reliability

Limitations:

Limited to sequential WAV playback (not truly polyphonic)
Older API design patterns
Less cross-platform compatibility

### Arduino Audio Tools
Strengths:

Modern C++ architecture
Cross-platform compatibility
Good for single-stream applications
Elegant streaming abstractions
Active development

Critical Issues:

Frequent compilation errors and API breaks
Poor performance on ESP32 (quality degradation)
Failed basic functionality (WAV playback didn't work)
High overhead from abstraction layers
Unstable (I2S crashes, buffer configuration issues)
Not optimized for embedded systems

Verdict
ESP8266Audio is superior for ESP32 audio projects requiring:

Multiple simultaneous audio streams
Production stability
Low latency
Quality audio output

Arduino Audio Tools is only suitable for:

Simple, single-stream playback
Cross-platform development
Learning modern audio programming concepts

### ESP32_S3_Sampler

For your polyphonic sampler goal, neither library is optimal. ESP32_S3_Sampler by copych is purpose-built for polyphonic sampling with up to 19 concurrent voices and direct SD streaming.
