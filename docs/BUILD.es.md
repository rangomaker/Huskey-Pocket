[English](BUILD.md) | **Español**

# Compilar el firmware

## Dependencias

La preparación del repositorio se verifica con las versiones instaladas localmente:

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

El sketch incluye su [lv_conf.h](../firmware/password_manager/lv_conf.h), recuperado de la instalación local del proyecto. Mantén esta configuración al compilar: activa color RGB565 sin intercambio de bytes, tick basado en `millis()`, QR, snapshots y las fuentes utilizadas. Si tu instalación de LVGL carga otro `lv_conf.h`, configura la biblioteca para utilizar el incluido en este sketch.

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

La tabla [partitions.csv](../firmware/password_manager/partitions.csv) se mantiene junto al sketch y debe usarse en la compilación. Su última partición termina en `0xE03000`, más allá de 8 MB; esta tabla requiere flash de 16 MB. Comprueba la memoria de tu placa antes de cargarla. USB-OTG habilita las clases HID y MSC utilizadas por el código.

## Arduino CLI

Desde la raíz del repositorio:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CPUFreq=240,FlashSize=16M,FlashMode=qio,PSRAM=opi,PartitionScheme=custom,USBMode=default,CDCOnBoot=cdc,UploadSpeed=921600,DebugLevel=none" --build-path .work/build firmware/password_manager
```

Los archivos generados quedan en `.work/build`, excluido de Git. El comando compila; no carga ni modifica un dispositivo conectado.

## Aviso de privacidad

Antes de distribuir tu firmware, completa [privacy_notice.h](../firmware/password_manager/privacy_notice.h) siguiendo [PRIVACY.es.md](PRIVACY.es.md). El repositorio incluye un mensaje visible de aviso pendiente de configurar en ambos idiomas.

## Verificación

El resultado de compilación y los límites de la comprobación se registran en [VALIDATION.md](VALIDATION.md). La compilación no comprueba pantalla, táctil, teclado, almacenamiento, RTC, IMU, batería ni estado de aprovisionamiento de una unidad física.
