#pragma once

// Debug configuration
#define DEBUG_ON

// Audio configuration
#define SAMPLE_RATE           44100
#define MAX_POLYPHONY         8           
#define MAX_SAMPLES           16          
#define MAX_INSTRUMENTS       4           // Maximum number of instruments

// I2S pins for UDA1334A DAC
#define I2S_BCLK_PIN    5
#define I2S_DOUT_PIN    6  
#define I2S_WCLK_PIN    7

// SD Card SPI pins
#define SD_CS_PIN    10
#define SD_MOSI_PIN  11
#define SD_MISO_PIN  13
#define SD_SCK_PIN   12

// SD Card 4 bit pins:
// #define SDMMC_CMD 3
// #define SDMMC_CLK 46 
// #define SDMMC_D0  9
// #define SDMMC_D1  10
// #define SDMMC_D2  11
// #define SDMMC_D3  12

// MIDI pins

#define MIDIRX_PIN      4
#define MIDITX_PIN      9

// Audio settings
#define DMA_BUF_LEN     256
#define DMA_NUM_BUF     8

// Global audio settings
extern float sampleVolume;
extern int loadedSamples;