**English** | [Español](README.es.md)

# Huskey Pocket

A hardware password manager built for **Waveshare ESP32-S3-Touch-LCD-2.8**. Store credentials locally, generate TOTP codes, and type data through USB or Bluetooth Low Energy HID without cloud accounts or service dependencies.

<p align="center">
  <img src="assets/renders/huskey-gray-three-quarter.png" width="360" alt="Gray Huskey Pocket">
  <img src="assets/renders/huskey-black-three-quarter.png" width="360" alt="Black Huskey Pocket">
</p>

> **Reference project, without active maintenance.** Shared under the MIT License, as is, without guaranteed support, fixes or feature updates. Forks and independent development are welcome.

## Features

### Credentials and organization

- Accounts with a title, username, password, notes/URL and TOTP secret.
- Secret and note records for codes, keys and private text.
- Folders and categories: create, rename, move entries and delete.
- Favorites and search by account name or username.
- Hidden sensitive fields with a reveal option and visual transfer through QR codes.

### PIN and vault

- A **6-digit master PIN**, with PIN changes from Settings and entry re-encryption.
- **AES-256-CBC** encryption with a random IV per operation and **50,000-iteration PBKDF2-HMAC-SHA256** key derivation.
- LittleFS vault storage and NVS device preferences.
- A persistent failed-attempt counter; **five consecutive incorrect PIN attempts reset the device and erase the vault**.
- A decoy PIN opens a session without access to real credentials, with simulated saves and a configurable timer that resets the device and erases the vault.
- Factory Reset Protection (FRP) tied to the previous PIN.

### Locking and physical protection

- Idle locking after **30 seconds, 1, 2 or 5 minutes**, or disabled.
- **Snatch Lock**: QMI8658C acceleration and rotation detection, followed by locking and Deep Sleep. Low, medium, high or disabled sensitivity.
- **Lost mode**: a factory reset at startup after **7, 14, 30, 60, 90 or 180 days** without a real-PIN unlock, depending on configuration.

### USB and Bluetooth keyboards

- **USB HID** and wireless **Bluetooth Low Energy HID** keyboard modes.
- **ES-LAT, ES-ES and US** layouts, including letter, symbol and modifier mappings.
- Type a username followed by Tab, a password followed by Enter, TOTP codes or generated passwords.
- Bluetooth pairing-code confirmation and bonded-device management from Settings.

### TOTP and password generation

- **6-digit** HMAC-SHA1 TOTP codes with **30-second** intervals, compatible with RFC 6238.
- A **PCF85063** clock keeps time without internet synchronization; an on-screen countdown shows code validity.
- An **8–32-character** generator with lowercase letters, optional uppercase letters, digits and symbols, and an ambiguity filter.
- A standalone generator and form-integrated generation, with direct typing into the connected device.

### Backups and transfer

- Export and import **encrypted `.hky` backups**, using a password independent of the PIN and **100,000-iteration PBKDF2**.
- **CSV** import/export for record migration. CSV files contain readable plaintext data.
- Block-based imports to keep the interface responsive and exported-file cleanup on microSD.
- **USB Mass Storage (MSC)** access to the microSD from a computer, with an optional read-only mode.
- Exclusive storage mode: the application and keyboard functions are suspended while MSC is active.

### Touch interface

- **Spanish and English** languages and **light, dark and custom** themes.
- Hue and saturation adjustments for the background, cards and accents.
- A **6-panel** split keyboard with Shift and Caps Lock, plus a **4 × 4 T9** keyboard with multitap and long-press input.
- Initial setup for language, privacy notice, date/time, keyboard layout, locking, theme and PIN.
- A status bar for time, battery, charging and microSD; toast messages and haptic feedback.

### Power and screenshots

- Lock, restart and shutdown controls, backlight management and Deep Sleep.
- **BMP screenshots on microSD** through the serial commands `shot`, `shot2` and `shot3`.
- Source code, manuals, whitepaper, render gallery, UI screens and editable Blender enclosure.

See [FEATURES.md](FEATURES.md) for the module breakdown.

## Getting started

1. Browse the [documentation index](docs/README.md) and user manual.
2. Open [password_manager.ino](firmware/password_manager/password_manager.ino) in Arduino IDE. Keep all sketch files together.
3. Install dependencies and configure the board using the [build guide](docs/BUILD.md).
4. Customize the [privacy notice](docs/PRIVACY.md) before distributing your implementation.

## Privacy notice

**Each person or organization adapting, implementing or distributing Huskey Pocket is responsible for writing, publishing and maintaining their own privacy notice.** Repository authors do not manage this on behalf of third parties.

[privacy_notice.h](firmware/password_manager/privacy_notice.h) holds the Spanish and English text displayed in onboarding and Settings. The default states **“Notice not configured”**; the checkbox acknowledges reading it. Replace the content with your implementation's notice and update its version. The code's MIT License remains in [LICENSE](LICENSE).

## Hardware and build

| Component | Project configuration |
| --- | --- |
| Microcontroller | ESP32-S3, 240 MHz |
| Flash | **16 MB** |
| PSRAM | OPI PSRAM, 8 MB |
| Display / touch | ST7789, 240 × 320 / CST328 |
| RTC / IMU | PCF85063 / QMI8658C |
| External storage | MicroSD, 1-bit SD_MMC |
| Battery | 3.7 V LiPo for portable operation |
| Haptics | Vibration motor |
| USB Mode | **USB-OTG (TinyUSB)** for HID and MSC |

## Required libraries

### Board package

| Package | Version | Installation |
| --- | --- | --- |
| [Espressif Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html), `esp32:esp32` | **3.3.10** | Arduino IDE Boards Manager |

### External libraries

Install these versions from the Arduino IDE **Library Manager**:

| Library Manager name | Version | Purpose |
| --- | --- | --- |
| [lvgl](https://github.com/lvgl/lvgl/tree/v8.3.6) | **8.3.6** | Touch interface, widgets, themes, QR and screenshots |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino/releases/tag/2.5.0) | **2.5.0** | Bluetooth Low Energy keyboard, pairing and bonded devices |

### ESP32 package components

| Component | Purpose |
| --- | --- |
| `SPI` and `Wire` | Display, touch, RTC and IMU buses |
| `FS`, `LittleFS` and `SD_MMC` | Internal vault and microSD files |
| `Preferences` | NVS settings and counters |
| `USB`, `USBHID` and `USBMSC` | USB keyboard and mass storage |
| `mbedTLS` | AES, SHA and PBKDF2 key derivation |
| `FreeRTOS` and ESP-IDF APIs | Tasks, sleep, GPIO and ESP32-S3 functions |

These components are installed with **Arduino-ESP32 3.3.10**. ST7789, CST328, PCF85063 and QMI8658C drivers, fonts and touch keyboards are in `firmware/password_manager/`.

Keep [lv_conf.h](firmware/password_manager/lv_conf.h) beside the sketch: it configures 16-bit color, the time tick, QR, snapshots and fonts. The [build guide](docs/BUILD.md) provides installation steps and all board settings.

## Interface

<p align="center">
  <img src="assets/ui/list-dark.png" width="180" alt="Dark account list">
  <img src="assets/ui/list-light.png" width="180" alt="Light account list">
  <img src="assets/ui/generator-dark.png" width="180" alt="Password generator">
  <img src="assets/ui/totp-setup-dark.png" width="180" alt="TOTP setup">
</p>

[View the full gallery](docs/GALLERY.md).

## Repository layout

```text
firmware/password_manager/   Arduino sketch, modules, fonts and lv_conf.h
docs/                       Guides and index
docs/reference/             Manuals and whitepaper (DOCX/PDF)
assets/renders/             Device renders
assets/ui/                  High-resolution UI references
hardware/CASE.blend         Blender enclosure source
FEATURES.es.md              Spanish feature description
FEATURES.md                 English feature description
LICENSE                     MIT License
```

## License

[MIT](LICENSE). Third-party dependencies and resources retain their respective licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
