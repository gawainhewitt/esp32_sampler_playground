# SD Card Configuration

## SDMMC 1-bit Mode

This project uses SDMMC (SD/MMC host controller) in 1-bit mode for SD card access.

### Why SDMMC Instead of SPI?
- **Faster:** 8-10 MB/s vs 2-5 MB/s with SPI
- **Lower CPU usage:** Hardware controller handles transfers
- **Better for streaming:** More consistent throughput for MP3 playback

### Why 1-bit Instead of 4-bit?
The ESP32-S3 has known reliability issues with 4-bit SDMMC mode:
- Works initially but fails after first batch of file operations
- Error codes: 0x107 (timeout), 0xffffffff (communication failure)
- Documented across multiple ESP32-S3 projects and GitHub issues
- 1-bit mode is stable and still significantly faster than SPI

### Hardware Connections

#### ESP32-S3-DevKitM-1 Pins
```
SD Card Pin → ESP32-S3 GPIO
CMD  (Pin 3) → GPIO 12
CLK  (Pin 5) → GPIO 11
D0   (Pin 7) → GPIO 10
VDD  (Pin 4) → 3.3V
GND  (Pin 6) → GND
```

**Note:** D1, D2, D3 are not used in 1-bit mode

#### Pull-up Resistors
- Most SD card breakout boards include 10kΩ pull-ups on CMD and D0
- If your breakout doesn't have them, add external 10kΩ pull-ups to 3.3V

#### SD Card Requirements
- Format as FAT32
- Class 10 or UHS-I recommended for smooth MP3 streaming
- Maximum 32GB for FAT32 (or use exFAT if supported)

### Software Configuration

Defined in `config.h`:
```cpp
#define SDMMC_CMD 12
#define SDMMC_CLK 11 
#define SDMMC_D0  10
```

Initialization in `sd_manager.cpp`:
```cpp
SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0);
SD_MMC.begin("/sdcard", true, false, 20000, 10);
//                      ^^^^
//                      true = 1-bit mode
```

### Performance Benchmarks

| Mode | Speed | CPU Usage | Reliability |
|------|-------|-----------|-------------|
| SPI | 2-5 MB/s | High | Good |
| SDMMC 1-bit | 8-10 MB/s | Low | Excellent |
| SDMMC 4-bit | 20-40 MB/s | Lowest | **Unstable on ESP32-S3** |

### Troubleshooting

#### "Failed to mount SD card"
- Check connections (CMD, CLK, D0, VDD, GND)
- Verify SD card is formatted as FAT32
- Try a different SD card
- Check 3.3V power supply

#### "No SD card attached"
- SD card not fully inserted
- Bad SD card socket contact

#### File operations fail after boot
- If using 4-bit mode, switch to 1-bit mode (set second parameter of `begin()` to `true`)

### References

Known ESP32-S3 SDMMC 4-bit issues:
- https://github.com/espressif/arduino-esp32/issues/7373
- https://github.com/espressif/arduino-esp32/issues/9274
- https://github.com/espressif/esp-idf/issues/8521
- https://esp32.com/viewtopic.php?t=23092

### Future Improvements

If 4-bit mode becomes reliable in future ESP-IDF/Arduino versions:
1. Change `SD_MMC.begin()` second parameter to `false`
2. Wire up D1 (GPIO 9), D2 (GPIO 14), D3 (GPIO 13)
3. Test thoroughly with multiple file operations

For now, 1-bit mode provides the best balance of speed and reliability.
