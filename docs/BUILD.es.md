[English](BUILD.md) | **Español**

# Compilar el firmware

## Dependencias

Versiones de las dependencias:

| Componente | Versión |
| --- | --- |
| Arduino-ESP32, paquete `esp32:esp32` | 3.3.10 |
| LVGL, librería `lvgl` | 8.3.6 |
| NimBLE-Arduino | 2.5.0 |

Instala el paquete ESP32 y ambas librerías desde Arduino IDE, o utiliza Arduino CLI:

```powershell
arduino-cli core install esp32:esp32@3.3.10
arduino-cli lib install "lvgl@8.3.6" "NimBLE-Arduino@2.5.0"
```

Usa [lv_conf.h](../firmware/password_manager/lv_conf.h) para configurar LVGL: color RGB565 sin intercambio de bytes, tick basado en `millis()`, QR, snapshots y fuentes de la interfaz.

Los controladores de pantalla, táctil, RTC, IMU, fuentes y módulos de aplicación están en el sketch. USB, SD_MMC, LittleFS, Preferences y mbedTLS provienen del paquete Arduino-ESP32.

## Arduino IDE

Abre [password_manager.ino](../firmware/password_manager/password_manager.ino). La carpeta se llama `password_manager`, igual que el archivo principal, para que Arduino conserve todos sus módulos juntos.

| Herramientas / Tools | Valor |
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

La tabla [partitions.csv](../firmware/password_manager/partitions.csv) requiere una flash de 16 MB. Mantén el archivo junto al sketch. Selecciona **USB-OTG (TinyUSB)** para habilitar el teclado HID y el almacenamiento MSC.

## Arduino CLI

Desde la raíz del repositorio:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CPUFreq=240,FlashSize=16M,FlashMode=qio,PSRAM=opi,PartitionScheme=custom,USBMode=default,CDCOnBoot=cdc,UploadSpeed=921600,DebugLevel=none" --build-path build firmware/password_manager
```

El firmware se genera en `build/`.

## Aviso de privacidad

Antes de distribuir tu firmware, completa [privacy_notice.h](../firmware/password_manager/privacy_notice.h) siguiendo [PRIVACY.es.md](PRIVACY.es.md). El repositorio incluye un mensaje visible de aviso pendiente de configurar en ambos idiomas.
