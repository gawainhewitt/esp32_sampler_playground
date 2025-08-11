# esp32_sampler_playground
test project for sampler functionality on an esp32

needs external power to run properly otherwise expect clicks and pops

if running on external power then ENABLE_SERIAL_DEBUG needs to be set to false or it will end up stuck in the while loop

next steps:

ok that's better but yes i would like to pre-initialise the wav system and then have it retain the sample in memory. How do i do that

Edit
Great idea! Here's how to pre-initialize and keep the WAV in memory for instant playback:

1. Modify Your Audio Variables
In audio.h, add a new variable:

cpp
extern bool wavSystemReady;  // Track if WAV system is initialized
In audio.cpp, add:

cpp
bool wavSystemReady = false;
2. Create WAV Pre-initialization Function
In audio.h, add:

cpp
void preInitializeWAVSystem();
In audio.cpp, add this new function:

cpp
void preInitializeWAVSystem() {
  debug_printf("Pre-initializing WAV system... \n");
  
  // Create WAV file source
  wavFileSource = new AudioFileSourceSD(WAV_FILE);
  if (!wavFileSource) {
    debug_printf("Failed to create WAV file source for pre-init \n");
    return;
  }
  
  // Create PSRAM buffer and fully load the WAV file
  wavBuffer = new AudioFileSourceBuffer(wavFileSource, WAV_BUFFER_SIZE);
  if (!wavBuffer) {
    debug_printf("Failed to create WAV PSRAM buffer for pre-init \n");
    delete wavFileSource;
    wavFileSource = nullptr;
    return;
  }
  
  // Create WAV generator but don't start it yet
  wavGenerator = new AudioGeneratorWAV();
  if (!wavGenerator) {
    debug_printf("Failed to create WAV generator for pre-init \n");
    delete wavBuffer;
    delete wavFileSource;
    wavBuffer = nullptr;
    wavFileSource = nullptr;
    return;
  }
  
  wavSystemReady = true;
  debug_printf("WAV system pre-initialized and ready for instant playback \n");
  debug_printf("Free PSRAM after WAV pre-init: %d bytes\n", ESP.getFreePsram());
}
3. Modify triggerWAVEffect()
Replace your current triggerWAVEffect() with:

cpp
void triggerWAVEffect() {
  if (wavPlaying) {
    debug_printf("WAV already playing, skipping trigger \n");
    return;
  }
  
  if (!wavSystemReady) {
    debug_printf("WAV system not ready! \n");
    return;
  }
  
  debug_printf("Triggering pre-loaded WAV effect... \n");
  
  // Restart the file from beginning
  wavFileSource->seek(0, SEEK_SET);
  
  // Start WAV playback (instant since everything is pre-loaded)
  if (wavGenerator->begin(wavBuffer, wavChannel)) {
    wavPlaying = true;
    debug_printf("WAV effect started instantly! \n");
  } else {
    debug_printf("Failed to start pre-loaded WAV playback \n");
  }
}
4. Call Pre-initialization in Setup
In main.cpp, add this to your setup() after the other audio initialization:

cpp
// Start background MP3
startMP3Background();

// Pre-initialize WAV system for instant playback
preInitializeWAVSystem();

// Create audio processing task on Core 1
5. Modify cleanupWAV()
Update cleanupWAV() to not destroy the pre-loaded system:

cpp
void cleanupWAV() {
  if (wavGenerator) {
    wavGenerator->stop();
    // Don't delete - keep for reuse!
  }
  wavPlaying = false;
  // Keep wavFileSource, wavBuffer, and wavGenerator loaded for next time
}
Now your WAV will be loaded into PSRAM at startup and trigger instantly when the button is pressed!




ok and later we can have a new load function to replace it if necessary?

Edit
Absolutely! That's exactly the right design approach. Here's how you can set it up:

Add a WAV Loading Function
In audio.h, add:

cpp
void loadWAVFile(const char* filename);
In audio.cpp, add:

cpp
void loadWAVFile(const char* filename) {
  debug_printf("Loading new WAV file: %s \n", filename);
  
  // Stop current playback if running
  if (wavPlaying) {
    wavGenerator->stop();
    wavPlaying = false;
  }
  
  // Clean up existing WAV system
  if (wavGenerator) {
    delete wavGenerator;
    wavGenerator = nullptr;
  }
  if (wavBuffer) {
    delete wavBuffer;
    wavBuffer = nullptr;
  }
  if (wavFileSource) {
    delete wavFileSource;
    wavFileSource = nullptr;
  }
  
  wavSystemReady = false;
  
  // Verify new file exists
  if (!SD.exists(filename)) {
    debug_printf("WAV file %s not found! \n", filename);
    return;
  }
  
  // Create new WAV system with the new file
  wavFileSource = new AudioFileSourceSD(filename);
  if (!wavFileSource) {
    debug_printf("Failed to create WAV file source for %s \n", filename);
    return;
  }
  
  wavBuffer = new AudioFileSourceBuffer(wavFileSource, WAV_BUFFER_SIZE);
  if (!wavBuffer) {
    debug_printf("Failed to create WAV buffer for %s \n", filename);
    delete wavFileSource;
    wavFileSource = nullptr;
    return;
  }
  
  wavGenerator = new AudioGeneratorWAV();
  if (!wavGenerator) {
    debug_printf("Failed to create WAV generator for %s \n", filename);
    delete wavBuffer;
    delete wavFileSource;
    wavBuffer = nullptr;
    wavFileSource = nullptr;
    return;
  }
  
  wavSystemReady = true;
  debug_printf("New WAV file %s loaded and ready! \n", filename);
  debug_printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
}
Usage Examples
cpp
// Later in your code, you can do:
loadWAVFile("/explosion.wav");
loadWAVFile("/beep.wav");
loadWAVFile("/music_stinger.wav");

// Button press will now trigger whatever was last loaded
Even Better - WAV Bank System
You could later expand this to a multi-sample system:

cpp
// Future possibilities:
loadWAVToSlot(0, "/kick.wav");
loadWAVToSlot(1, "/snare.wav");
loadWAVToSlot(2, "/hihat.wav");

triggerWAVSlot(0);  // Play kick
triggerWAVSlot(1);  // Play snare
Perfect for building a drum machine or sound effects board! The foundation you're building now supports all of this future expansion.