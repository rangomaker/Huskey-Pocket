[English](FEATURES.md) | [Espanol](FEATURES.es.md)

---

# KeyVault / Huskey Pocket - Technical Feature Specification

Technical reference detailing the architecture, capabilities, and operational features of the offline hardware password manager built for the Waveshare ESP32-S3-Touch-LCD-2.8 platform.

---

## 1. Overview and Hardware Architecture

The device functions as an offline cryptographic vault for the secure storage, retrieval, and injection of digital credentials without any cloud or internet dependencies.

* **Microcontroller**: Espressif ESP32-S3 (Dual-Core Xtensa LX7 up to 240 MHz with integrated Flash and PSRAM).
* **Display**: 2.8-inch IPS TFT LCD, 240x320 resolution, driven by an ST7789 controller over SPI at 80 MHz.
* **Touch Digitizer**: CST328 capacitive touch controller over a dedicated I2C interface (Wire1 bus) supporting continuous gestures and multi-region taps.
* **Real-Time Clock (RTC)**: Dedicated PCF85063 chip on the primary I2C interface (Wire bus). Requires an external backup battery to retain the date and time while the device is powered off and support TOTP across power cycles.
* **Battery and Enclosure**: Main 3.7 V LiPo battery in the **523450** format. The enclosure is designed for this format; RTC backup uses a separate external battery.
* **Inertial Measurement Unit (IMU)**: QMI8658C 6-axis accelerometer and gyroscope over I2C (Wire bus) for real-time dynamic motion and tilt monitoring.
* **Removable Storage**: MicroSD card slot wired in native 1-bit SD_MMC mode for high-speed transfers and electrical isolation.
* **Haptic Feedback**: Coreless ERM vibration motor driven by an onboard transistor to provide physical feedback on keypad taps and system confirmations.
* **Power Management**: Battery voltage sensing via calibrated ADC, peripheral power cutoff through an AO3401 P-channel MOSFET, and Deep Sleep power states.

---

## 2. Cryptographic Security and Vault Architecture

* **Encryption Algorithm**: AES-256-CBC implemented with mbedTLS. CBC does not provide authenticated encryption.
* **Key Derivation**: Master encryption key derived from the user PIN using PBKDF2-HMAC-SHA256 with 50,000 iterations and a unique hardware-generated salt (`esp_random`). Encrypted `.hky` backups use 100,000 iterations with a separate backup password.
* **PIN Authentication**: Master access protected by a 6-digit numerical PIN (`PIN_DIGITS` in `vault.h`). PIN verification utilizes constant-time memory comparisons to prevent timing side-channel attacks.
* **PIN Attempts**: A persistent NVS counter records failed attempts. The fourth failure displays a warning; the fifth consecutive failure resets the device and erases the vault.
* **Decoy PIN**: Opens an empty vault without access to real records. Saves are simulated, and a configurable factory-reset timer erases the vault.
* **Factory Reset Protection (FRP)**: The previous PIN is required after a reset. Three failed FRP checks activate the firmware's permanent lockout.
* **Secure Storage**: Encrypted entries reside in the on-chip LittleFS flash partition, which provides wear-leveling and power-failure resilience. System parameters are kept in NVS (Non-Volatile Storage).
* **Secure Wipe**: Factory reset clears credentials and key material. Flash wear leveling can retain physical copies; erasure is not a guarantee against laboratory recovery.

---

## 3. Active Physical Defense

* **Anti-Snatch Protection (Snatch Lock)**:
  * Continuous vector analysis of linear acceleration and angular velocity using the QMI8658C IMU.
  * If a sudden, correlated spike in acceleration and rotation is detected (characteristic of a physical snatch or grab), the firmware calls the vault lock routine and enters Deep Sleep. End-to-end latency must be measured on the target hardware.
  * Sensitivity levels: Disabled, Low, Medium, and High.
* **Dead Man's Switch**:
  * Select 7, 14, 30, 60, 90 or 180 days, or disable the feature.
  * At startup, if the interval has elapsed without a real-PIN unlock, a factory reset erases the vault. Decoy-PIN sessions do not renew the interval.
* **Automatic Screen Lock**: Select 30 seconds, 1, 2 or 5 minutes, or disable it. The idle timer turns off the screen and locks access.

---

## 4. HID Keyboard Emulation (Credential Injection)

The device directly types passwords and credentials into any connected PC, Mac, or smartphone by emulating a standard physical keyboard without requiring host-side software.

* **USB HID Mode**:
  * Utilizes the native ESP32-S3 TinyUSB stack to enumerate as a generic Human Interface Device (HID).
* **Bluetooth Low Energy (BLE) Mode**:
  * Wireless HID over GATT keyboard emulation powered by the NimBLE-Arduino library.
  * Compatible with Windows, macOS, Linux, Android, and iOS.
  * Supports encrypted bonding for seamless and secure reconnection.
* **Keyboard Layout Support**:
  * Native translation for ES-LAT (Spanish Latin America), ES-ES (Spanish Spain), and US (English United States) layouts.
  * Automatically maps accents, punctuation, uppercase symbols, and special characters across layouts.
* **One-Tap Injection Actions**:
  * Types Username followed by an automatic `Tab` key.
  * Types Password followed by an automatic `Enter` key.
  * Injects active TOTP two-factor verification codes directly.
  * Types freshly generated passwords from the on-screen generator.

---

## 5. Account and Credential Management

* **Supported Entry Types**:
  * **Web and Service Accounts**: Service Name, Username, Password, URL/Notes, and TOTP Base32 secret key.
  * **Secrets and Secure Notes**: Multiline freeform text records for cryptocurrency seed phrases, private keys, recovery codes, or sensitive memos.
* **Folder and Category Organization**:
  * Custom categories with support for creation, renaming, and deletion. Deleting a category safely reassigns entries to the default Uncategorized folder.
* **Favorites**:
  * Quick-marking entries with a favorite star for immediate access from the top of the main vault view.
* **Real-Time Search**:
  * Incremental instant-search filtering entries as characters are typed, with a dedicated single-tap clear button.
* **View Modes**:
  * Entry list or a three-column icon grid.
  * View preferences saved across sessions.
* **Filters and Sorting**:
  * Folder/category and favorites filters; search by name or username.
  * Name A-Z, name Z-A, newest creation date or oldest creation date.
  * Sorting preference persisted in NVS.
* **Multiple Selection**:
  * Select several entries, select all entries in the current view and batch delete.
* **Protected Password Visibility**:
  * Passwords are masked (`••••••`) by default. An eye toggle button temporarily reveals plain text accompanied by haptic feedback.
* **Visual QR Code Sharing**:
  * Generates high-contrast, black-and-white on-screen QR codes, allowing mobile devices to scan credentials without physical cables or wireless data exchanges.

---

## 6. Two-Factor Authentication (TOTP / 2FA)

* **Standard Compliance**: Fully compliant with RFC 6238 (HMAC-SHA1, 6-digit tokens, 30-second validity window), drop-in compatible with Google Authenticator and Microsoft Authenticator.
* **Network-Independent Clock**:
  * Token calculations use the PCF85063 RTC and the configured time-zone offset.
  * An external RTC backup battery is required to retain the date and time while the device is powered off.
* **Dynamic On-Screen Timer**:
  * Real-time visual countdown wheel and numerical display tracking seconds remaining before token regeneration.
* **Automated Typing**:
  * One-tap keyboard icon button types the active 6-digit token directly into host login prompts over USB or Bluetooth.

---

## 7. Integrated Password Generator

* **Length Control**: Configurable between 8 and 32 characters via an on-screen slider.
* **Character Set Selectors**:
  * Uppercase letters (A-Z).
  * Lowercase letters (a-z), always included as the base character set.
  * Numbers (0-9).
  * Special symbols and punctuation.
* **Ambiguity Filter**: Optional toggle to exclude visually confusing characters (such as `1`, `l`, `I`, `0`, and `O`).
* **Operational Modes**:
  * Standalone generator view accessible from the main navigation bar.
  * Embedded generator accessible directly within account creation and editing forms.

---

## 8. Virtual On-Screen Keyboards

* **Split QWERTY Keyboard (6 Sliding Panels)**:
  * Designed for the 2.8-inch display: Left Letters, Right Letters, Left Symbols 1, Right Symbols 1, Left Symbols 2, Right Symbols 2.
  * Single tap for one-shot uppercase; double-tap for persistent Caps Lock.
  * Fluid horizontal slide animations between layouts.
* **T9 Matrix Keypad (4x4 Grid)**:
  * Retro 16-key telephone layout with large touch targets.
  * Multitap cycling for letters (e.g., tap 2 repeatedly for A, B, C).
  * Long-press detection to type direct numbers, `.com`, or space.
  * QWERTY/T9 keyboard selection during onboarding and from Settings.

---

## 9. Storage, Backup, and Data Transfer

* **Encrypted Backups (`.hky` - Huskey Backup)**:
  * Complete vault backup saved to MicroSD, encrypted with AES-256 and an independent user-defined password.
* **Plaintext Export (CSV)**:
  * Standard comma-separated tabular export for migration to desktop managers (Bitwarden, KeePass, Excel), preceded by explicit on-screen security warnings.
* **Non-Blocking Asynchronous Import**:
  * Segmented timed parsing for `.hky` and `.csv` files that keeps the LVGL graphical interface responsive without freezing during large imports.
* **Secure Card Sanitization**:
  * Dedicated button in Settings to securely shred and delete the `/export` directory on the MicroSD card.
* **USB Mass Storage (MSC) Mode**:
  * Exposes the MicroSD card as a standard USB flash drive over the USB-C port, allowing direct file transfer without removing the physical card.

---

## 10. Interface, onboarding and personalization

### Initial setup wizard

Onboarding presents the following screens:

| Screen | Options or purpose |
| --- | --- |
| Language | Spanish or English |
| Welcome | Device introduction |
| Privacy notice | Implementation-specific text and reading acknowledgement |
| Date | Day, month and year |
| Time | Hours, minutes and 12-hour or 24-hour format |
| Keyboard | Input language and QWERTY or T9 type |
| Automatic locking | 30 seconds, 1, 2 or 5 minutes, or disabled |
| Theme | Appearance selection |
| Create PIN | A 6-digit master PIN |
| Confirm PIN | Verification of the selected PIN |
| Preparation | Vault storage creation progress |
| Completion | End of initial setup |

### Date, time and time zone

- Manual day, month and year adjustment from the date screen in Settings.
- Hour and minute adjustment and 12-hour or 24-hour format selection.
- PCF85063 clock and status-bar time display.
- External RTC backup battery, required to retain the date and time and support TOTP across power cycles.
- Time-zone offsets from UTC−12 to UTC+14 in 30-minute steps.
- Device time for TOTP and elapsed time for Lost mode.

### Personalization

- Light, dark and custom themes.
- Light or dark bases for the custom theme.
- Independent background, card and accent editing through hue and saturation.
- Preset accent colors: blue, green, orange, pink, purple and cyan.
- Palette preview while editing and text-color adjustment based on the background.
- Spanish or English interface language, saved in device preferences.

### Controls and indicators

- Status bar with a clock, battery level and charging animation.
- Toasts for results, errors and confirmations.
- Haptic feedback for interface actions.
- Power menu for locking, restarting or shutting down.
- QWERTY/T9 touch-keyboard and input-language selection from Settings.

## 11. Implementation-specific privacy notice

The implementer is responsible for writing, publishing and maintaining their own notice. Spanish and English text in `firmware/password_manager/privacy_notice.h` is shared by onboarding and Settings. The default message explicitly states that the notice has not been configured; see [PRIVACY.md](docs/PRIVACY.md).

## 12. Libraries and tools

| Component | Version | Purpose |
| --- | --- | --- |
| Arduino-ESP32 (`esp32:esp32`) | 3.3.10 | Board core, compiler and ESP-IDF APIs |
| LVGL (`lvgl`) | 8.3.6 | Interface, controls, QR and screenshots |
| NimBLE-Arduino | 2.5.0 | BLE HID keyboard and pairing |

Arduino-ESP32 supplies `SPI`, `Wire`, `FS`, `LittleFS`, `SD_MMC`, `Preferences`, `USB`, `USBHID`, `USBMSC`, mbedTLS and FreeRTOS. ST7789, CST328, PCF85063 and QMI8658C drivers compile from the sketch files.

`lv_conf.h` enables 16-bit color, a `millis()` tick source, QR, snapshots, user data and interface fonts. See [dependency installation](docs/BUILD.md) for steps and build settings.

## 13. Battery, power and screenshots

| Function | Behavior |
| --- | --- |
| Measurement | Calibrated LiPo voltage readings through the ADC, with sample averaging |
| Level | Voltage-based estimation, shown in 25% steps in the battery icon |
| Charging | Voltage-trend estimation and charging-segment animation |
| Low battery | Indicator color change at 15% or below when not charging |
| Reading stability | Fast/slow filtering and charging-transition compensation |
| Power | Lock, restart, shutdown, backlight control and Deep Sleep |

Screenshots are saved as BMP files under `/export/` on microSD. Serial commands are `shot` for original size, `shot2` for 2× scale and `shot3` for 3× scale.

## 14. Factory reset and FRP

- Manual reset starts from **Settings → Factory reset**.
- It requires confirmation, entry of exactly **`Reset`** and a **5-second** press-and-hold.
- Deletes vault entries, the access PIN, folders/categories, preferences and decoy configuration.
- FRP verification remains stored after the reset.
- Before device setup resumes, the FRP screen requests the previously registered real PIN.
- The FRP attempt counter is persistent. The third incorrect attempt activates the firmware's permanent lockout.

## 15. Decoy PIN

- Configured with a PIN different from the real PIN.
- Entering it displays an empty vault, and session saves are simulated.
- Real records are not loaded into the decoy session.
- The reset timer offers **2, 5 or 10 minutes**, with a 5-minute default.
- Timer expiration resets the device and erases the vault; FRP remains active.
- From the real session, Settings allows enabling, changing or disabling the decoy PIN and adjusting its timer.
