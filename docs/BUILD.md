**English** | [Español](BUILD.es.md)

# Build the firmware

## Batteries and enclosure

- **Main battery:** **523450** format 3.7 V LiPo. The [enclosure](../hardware/CASE.blend) is designed to accommodate this format.
- **RTC backup:** date, time and TOTP functions require an **external PCF85063 backup battery**, separate from the main battery. This backup retains the date and time while the device is powered off.

## Board package

Install **esp32 by Espressif Systems 3.3.10** through Boards Manager. This package supplies the Arduino-ESP32 core, compiler and ESP-IDF APIs.

1. Open **File → Preferences** in Arduino IDE.
2. Add this index under **Additional Boards Manager URLs**:

   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

3. Open **Tools → Board → Boards Manager** and search for **esp32 by Espressif Systems**.
4. Select and install version **3.3.10**.
5. Select **ESP32S3 Dev Module**.

[Espressif installation documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).

## External libraries

Open Arduino IDE **Library Manager** and install:

| Name | Author | Version | Purpose |
| --- | --- | --- | --- |
| [lvgl](https://github.com/lvgl/lvgl/tree/v8.3.6) | LVGL / kisvegabor | **8.3.6** | Screens, controls, themes, fonts, QR and snapshots |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino/releases/tag/2.5.0) | h2zero | **2.5.0** | BLE HID keyboard, pairing and bonds |

Use these versions to compile against the APIs used by this firmware.

## ESP32 package libraries and APIs

These components are installed with **Arduino-ESP32 3.3.10**:

| Component | Headers or APIs | Huskey Pocket usage |
| --- | --- | --- |
| SPI | `SPI.h` | ST7789 display |
| I²C | `Wire.h`, `Wire`, `Wire1` | CST328 touch, PCF85063 RTC and QMI8658C IMU |
| Filesystems | `FS.h`, `LittleFS.h`, `SD_MMC.h` | Internal vault and microSD |
| NVS | `Preferences.h` | Settings, counters and control values |
| USB / TinyUSB | `USB.h`, `USBHID.h`, `USBMSC.h` | HID keyboard and USB storage mode |
| mbedTLS | `aes.h`, `sha256.h`, `pkcs5.h`, `md.h` | AES-256-CBC, SHA, HMAC and PBKDF2 |
| FreeRTOS | Tasks and timers | Key derivation and system execution |
| ESP-IDF | Sleep, GPIO, eFuse and MAC | Power management and ESP32-S3 hardware functions |

## Sketch drivers and modules

The following files compile as part of the project:

| Files | Purpose |
| --- | --- |
| `Display_ST7789.h/.cpp` | Display and backlight driver |
| `Touch_CST328.h/.cpp` | Touch driver |
| `rtc_manager.h` | PCF85063 RTC |
| `snatch_lock.h` | QMI8658C IMU and Snatch Lock |
| `inter_*.c`, `material_16*.c`, `fonts.h` | Fonts and glyphs |
| `custom_kb.h`, `custom_kb_t9.h`, `kb_manager.h` | Touch keyboards |
| `battery_manager.h`, `power_manager.h`, `vibro_manager.h` | Battery, power and vibration |

## LVGL configuration

Keep [lv_conf.h](../firmware/password_manager/lv_conf.h) inside `firmware/password_manager/`, beside the sketch. Its main options are:

| Option | Value | Purpose |
| --- | --- | --- |
| `LV_COLOR_DEPTH` | `16` | RGB565 color |
| `LV_COLOR_16_SWAP` | `0` | Byte order used by the driver |
| `LV_TICK_CUSTOM` | `1` | System time through `millis()` |
| `LV_USE_USER_DATA` | `1` | Object and event user data |
| `LV_USE_QRCODE` | `1` | QR code generation |
| `LV_USE_SNAPSHOT` | `1` | Screenshots |
| `LV_MEM_CUSTOM` | `1` | Allocation through `malloc`, `free` and `realloc` |

Retain the fonts and remaining options in this file so LVGL modules and the sketch share the same configuration.

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

Install the dependencies:

```powershell
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.10 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib update-index
arduino-cli lib install "lvgl@8.3.6" "NimBLE-Arduino@2.5.0"
```

Compile from the repository root:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CPUFreq=240,FlashSize=16M,FlashMode=qio,PSRAM=opi,PartitionScheme=custom,USBMode=default,CDCOnBoot=cdc,UploadSpeed=921600,DebugLevel=none" --build-path build firmware/password_manager
```

Firmware output is written to `build/`.

## Device checks

After uploading firmware **1.0.1**, check:

1. **Sliding keyboard:** switch repeatedly between letters and symbols, including while a panel transition finishes. Returning to letters must leave the keyboard in letter mode.
2. **T9:** enter a character and press **OK** before the multi-tap timeout. The character must be retained and the field confirmed.
3. **MicroSD:** copy a CSV into `/import` and restart. The access screen appears before SD mounting and cleanup; the CSV is retained and becomes available for import when initialization finishes.

The serial monitor at **115200 baud** reports separate `[SD] Montaje terminado` and `[SD] Limpieza de arranque terminada` messages, including elapsed milliseconds.

## Privacy notice

Before distributing firmware, complete [privacy_notice.h](../firmware/password_manager/privacy_notice.h) using [PRIVACY.md](PRIVACY.md). The repository includes a visible notice-not-configured message in both languages.
