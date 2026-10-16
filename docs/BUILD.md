**English** | [Español](BUILD.es.md)

# Build the firmware

## Dependencies

Repository preparation uses the versions installed locally:

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

The sketch includes [lv_conf.h](../firmware/password_manager/lv_conf.h), recovered from the project's local installation. Retain this configuration: it enables RGB565 without byte swapping, a `millis()` tick source, QR, snapshots and the required fonts. If your LVGL installation loads another configuration, configure it to use the header included in this sketch.

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

Keep [partitions.csv](../firmware/password_manager/partitions.csv) beside the sketch and use it in the build. Its last partition ends at `0xE03000`, beyond 8 MB; this table requires 16 MB flash. Check your board's memory before flashing. USB-OTG enables the HID and MSC classes used by the source.

## Arduino CLI

From the repository root:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CPUFreq=240,FlashSize=16M,FlashMode=qio,PSRAM=opi,PartitionScheme=custom,USBMode=default,CDCOnBoot=cdc,UploadSpeed=921600,DebugLevel=none" --build-path .work/build firmware/password_manager
```

Outputs go to `.work/build`, excluded from Git. This compiles without flashing or modifying a connected device.

## Privacy notice

Before distributing firmware, complete [privacy_notice.h](../firmware/password_manager/privacy_notice.h) using [PRIVACY.md](PRIVACY.md). The repository includes a visible notice-not-configured message in both languages.

## Verification

Build results and verification limits are recorded in [VALIDATION.md](VALIDATION.md). Compilation does not test the display, touch, keyboard, storage, RTC, IMU, battery or a physical device's provisioning state.
