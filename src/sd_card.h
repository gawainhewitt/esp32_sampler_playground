#pragma once

#include <SD.h>
#include <SPI.h>

// SD Card SPI pins
#define SD_CS_PIN    10
#define SD_MOSI_PIN  11
#define SD_MISO_PIN  13
#define SD_SCK_PIN   12

void setupSDCard();