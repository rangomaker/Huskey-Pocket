**English** | [Español](BUILD.es.md)

# Build the firmware

## Dependencies

Dependency versions:

| Component | Version |
| --- | --- |
| Arduino-ESP32, package `esp32:esp32` | 3.3.10 |
| LVGL, library `lvgl` | 8.3.6 |
| NimBLE-Arduino | 2.5.0 |

Install the ESP32 package and both libraries in Arduino IDE, or use Arduino CLI:

```powershell
arduino-cli core install esp32:esp32@3.3.10
arduino-cli lib install "lvgl@8.3.6" "NimBLE-Arduino@2.5.0"
```

Use [lv_conf.h](../firmware/password_manager/lv_conf.h) to configure LVGL: RGB565 without byte swapping, a `millis()` tick source, QR, snapshots and interface fonts.

Display, touch, RTC and IMU drivers, fonts and application modules are included. USB, SD_MMC, LittleFS, Preferences and mbedTLS come from Arduino-ESP32.

## Arduino IDE

Open [password_manager.ino](../firmware/password_manager/password_manager.ino). The folder matches the sketch name so Arduino keeps all modules together.

| Tools setting | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| CPU Frequency | 240MHz (WiFi/BT) |
| Flash Size | 16MB (128Mb) |
| Flash Mode | QIO 80MHz |
| PSRAM | OPI PSRAM |
| Partition Scheme | Custom |
| USB Mode | USB-OTG (TinyUSB) |
| USB CDC On Boot | Enabled |
| Upload Speed | 921600 |
| Core Debug Level | None |

The [partitions.csv](../firmware/password_manager/partitions.csv) table requires 16 MB flash. Keep it beside the sketch. Select **USB-OTG (TinyUSB)** to enable the HID keyboard and MSC storage.

## Arduino CLI

From the repository root:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CPUFreq=240,FlashSize=16M,FlashMode=qio,PSRAM=opi,PartitionScheme=custom,USBMode=default,CDCOnBoot=cdc,UploadSpeed=921600,DebugLevel=none" --build-path build firmware/password_manager
```

Firmware output is written to `build/`.

## Privacy notice

Before distributing firmware, complete [privacy_notice.h](../firmware/password_manager/privacy_notice.h) using [PRIVACY.md](PRIVACY.md). The repository includes a visible notice-not-configured message in both languages.
