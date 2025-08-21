#pragma once

// Debug configuration
#define DEBUG_ON

// Audio configuration
#define SAMPLE_RATE           44100
#define MAX_POLYPHONY         8           
#define MAX_SAMPLES           16          
#define MAX_INSTRUMENTS       4           // Maximum number of instruments

// I2S pins for UDA1334A DAC
#define I2S_BCLK_PIN    42
#define I2S_DOUT_PIN    41  
#define I2S_WCLK_PIN    2

// SD Card SPI pins
#define SD_CS_PIN    10
#define SD_MOSI_PIN  11
#define SD_MISO_PIN  13
#define SD_SCK_PIN   12

// MIDI pins
#define MIDIRX_PIN      4
#define MIDITX_PIN      9

// Audio settings
#define DMA_BUF_LEN     64
#define DMA_NUM_BUF     2

// Global audio settings
extern float sampleVolume;
extern int loadedSamples;