#pragma once

#include "constants.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "AudioOutputMixer.h"
#include "AudioFileSourceBuffer.h"

// Audio files
#define MP3_FILE "/sault.mp3"
#define WAV_FILE "/001Music.wav"

// Large PSRAM buffers for smooth audio
#define MP3_BUFFER_SIZE (256 * 1024)  // 128KB for MP3 (larger for continuous playback) ****I CHANGE THESE
#define WAV_BUFFER_SIZE (128 * 1024)   // 64KB for WAV effects
#define MIXER_BUFFER_SIZE 1024        // Mixer buffer size

// Audio system objects
extern AudioOutputI2S *i2s;
extern AudioOutputMixer *mixer;
extern AudioOutputMixerStub *mp3Channel;
extern AudioOutputMixerStub *wavChannel;

// MP3 background music (continuous)
extern AudioFileSourceSD *mp3FileSource;
extern AudioFileSourceBuffer *mp3Buffer;
extern AudioGeneratorMP3 *mp3Generator;

// WAV sound effects (triggered)
extern AudioFileSourceSD *wavFileSource;
extern AudioFileSourceBuffer *wavBuffer;
extern AudioGeneratorWAV *wavGenerator;

// Timing and state
extern unsigned long lastWavTime;
extern bool mp3Playing;
extern bool wavPlaying;

// Task handles for dual-core processing
extern TaskHandle_t audioTaskHandle;

extern bool mp3ShouldLoop;

// Function prototypes
void verifyAudioFiles();
void initializeAudioSystem();
void createAudioTask();
void startMP3Background();
void triggerWAVEffect();
void audioProcessingTask(void *parameter);
void restartMP3();
void cleanupMP3();
void cleanupWAV();
void setMP3Looping(bool shouldLoop);

