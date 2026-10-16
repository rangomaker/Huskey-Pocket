**English** | [Español](README.es.md)

# Huskey Pocket

A hardware password manager built for **Waveshare ESP32-S3-Touch-LCD-2.8**. Store credentials locally, generate TOTP codes, and type data through USB or Bluetooth Low Energy HID without cloud accounts or service dependencies.

<p align="center">
  <img src="assets/renders/huskey-gray-three-quarter.png" width="360" alt="Gray Huskey Pocket">
  <img src="assets/renders/huskey-black-three-quarter.png" width="360" alt="Black Huskey Pocket">
</p>

> **Reference project, without active maintenance.** Shared under the MIT License, as is, without guaranteed support, fixes or feature updates. Forks and independent development are welcome.

## Features and resources

- AES-256-CBC vault with PIN-based PBKDF2-HMAC-SHA256 key derivation.
- A 6-digit master PIN in this version, idle locking, decoy PIN and failed-attempt protections.
- USB and BLE HID keyboards with ES-LAT, ES-ES and US layouts.
- TOTP, password generator, notes, folders, favorites and QR data display.
- Encrypted `.hky` backups, CSV import/export and USB MSC access to microSD.
- IMU-based Snatch Lock and lost mode with erasure after prolonged inactivity.
- Spanish/English interface, light/dark/custom themes, split and T9 keyboards.
- Source code, manuals, whitepaper, renders, UI references and Blender enclosure source.

See [FEATURES.md](FEATURES.md). Physical and security behavior requires validation for each implementation.

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
| Flash | **16 MB for the included partition table** |
| PSRAM | OPI PSRAM, 8 MB according to project resources |
| Display / touch | ST7789, 240 × 320 / CST328 |
| RTC / IMU | PCF85063 / QMI8658C |
| External storage | MicroSD, 1-bit SD_MMC |
| Dependencies | Arduino-ESP32 3.3.10, LVGL 8.3.6, NimBLE-Arduino 2.5.0 |
| USB Mode | **USB-OTG (TinyUSB)** for HID and MSC |

Original resources mention 8 MB flash; that capacity cannot hold the included `partitions.csv`. The [build guide](docs/BUILD.md) covers settings and verification.

## Interface

<p align="center">
  <img src="assets/ui/list-dark.png" width="180" alt="Dark account list">
  <img src="assets/ui/list-light.png" width="180" alt="Light account list">
  <img src="assets/ui/generator-dark.png" width="180" alt="Password generator">
  <img src="assets/ui/totp-setup-dark.png" width="180" alt="TOTP setup">
</p>

Visual references supplied with the project. More views in the [gallery](docs/GALLERY.md).

## Repository layout

```text
firmware/password_manager/   Arduino sketch, modules, fonts and lv_conf.h
docs/                       Current guides and index
docs/reference/             Original manuals and whitepaper (DOCX/PDF)
assets/renders/             Device renders
assets/ui/                  High-resolution UI references
hardware/CASE.blend         Blender enclosure source
FEATURES.es.md              Spanish feature description
FEATURES.md                 English feature description
LICENSE                     Existing MIT License
```

Original manuals are preserved unchanged and may describe other revisions or provisioned devices. Use the current guide and included code to build this copy.

## License

[MIT](LICENSE). Third-party dependencies and resources retain their respective licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
