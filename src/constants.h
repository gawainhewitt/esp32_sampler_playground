#pragma once

// debugging
#define ENABLE_SERIAL_DEBUG true  

// I2S pins for UDA1334A
#define I2S_BCLK_PIN 42
#define I2S_WCLK_PIN 2
#define I2S_DOUT_PIN 41

// SD Card SPI pins
#define SD_CS_PIN    10
#define SD_MOSI_PIN  11
#define SD_MISO_PIN  13
#define SD_SCK_PIN   12

// Button pin
#define BUTTON_PIN 15

// Audio buffer sizes
#define MP3_BUFFER_SIZE (128 * 1024) // the first number is buffer size in kb
#define WAV_BUFFER_SIZE (64 * 1024) // the first number is buffer size in kb
#define MIXER_BUFFER_SIZE 1024 // Mixer buffer size

// === SYSTEM CONSTANTS ===
#define HEALTH_CHECK_INTERVAL 10000
#define MAIN_LOOP_DELAY 0