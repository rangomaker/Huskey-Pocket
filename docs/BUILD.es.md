[English](BUILD.md) | **Español**

# Compilar el firmware

## Baterías y carcasa

- **Batería principal:** LiPo de 3.7 V, formato **523450**. La [carcasa](../hardware/CASE.blend) está diseñada para alojar este formato.
- **Respaldo del RTC:** las funciones de fecha, hora y TOTP requieren una **batería externa de respaldo para el PCF85063**, independiente de la batería principal. Este respaldo conserva la fecha y la hora cuando el dispositivo está apagado.

## Paquete de la placa

Instala **esp32 by Espressif Systems 3.3.10** desde el Gestor de placas. Este paquete contiene el núcleo Arduino-ESP32, el compilador y las APIs ESP-IDF.

1. Abre **Archivo → Preferencias** en Arduino IDE.
2. Añade este índice en **URLs adicionales de Gestor de placas**:

   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

3. Abre **Herramientas → Placa → Gestor de placas** y busca **esp32 by Espressif Systems**.
4. Selecciona la versión **3.3.10** e instálala.
5. Selecciona **ESP32S3 Dev Module**.

[Documentación de instalación de Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).

## Librerías externas

Abre el **Gestor de bibliotecas** de Arduino IDE e instala:

| Nombre | Autor | Versión | Uso |
| --- | --- | --- | --- |
| [lvgl](https://github.com/lvgl/lvgl/tree/v8.3.6) | LVGL / kisvegabor | **8.3.6** | Pantallas, controles, temas, fuentes, QR y snapshots |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino/releases/tag/2.5.0) | h2zero | **2.5.0** | Teclado BLE HID, emparejamiento y vínculos |

Usa las versiones indicadas para compilar con las APIs de este firmware.

## Librerías y APIs del paquete ESP32

Estos componentes se instalan con **Arduino-ESP32 3.3.10**:

| Componente | Archivos o APIs | Uso en Huskey Pocket |
| --- | --- | --- |
| SPI | `SPI.h` | Pantalla ST7789 |
| I²C | `Wire.h`, `Wire`, `Wire1` | Táctil CST328, RTC PCF85063 e IMU QMI8658C |
| Archivos | `FS.h`, `LittleFS.h`, `SD_MMC.h` | Bóveda interna y microSD |
| NVS | `Preferences.h` | Ajustes, contadores y valores de control |
| USB / TinyUSB | `USB.h`, `USBHID.h`, `USBMSC.h` | Teclado HID y modo memoria USB |
| mbedTLS | `aes.h`, `sha256.h`, `pkcs5.h`, `md.h` | AES-256-CBC, SHA, HMAC y PBKDF2 |
| FreeRTOS | Tareas y temporizadores | Derivación de claves y ejecución del sistema |
| ESP-IDF | Suspensión, GPIO, eFuse y MAC | Energía y funciones de hardware del ESP32-S3 |

## Controladores y módulos del sketch

Los siguientes archivos se compilan como parte del proyecto:

| Archivos | Función |
| --- | --- |
| `Display_ST7789.h/.cpp` | Controlador de pantalla y retroiluminación |
| `Touch_CST328.h/.cpp` | Controlador táctil |
| `rtc_manager.h` | RTC PCF85063 |
| `snatch_lock.h` | IMU QMI8658C y Snatch Lock |
| `inter_*.c`, `material_16*.c`, `fonts.h` | Fuentes y glifos |
| `custom_kb.h`, `custom_kb_t9.h`, `kb_manager.h` | Teclados táctiles |
| `battery_manager.h`, `power_manager.h`, `vibro_manager.h` | Batería, energía y vibración |

## Configuración de LVGL

Mantén [lv_conf.h](../firmware/password_manager/lv_conf.h) dentro de `firmware/password_manager/`, junto al sketch. Sus opciones principales son:

| Opción | Valor | Función |
| --- | --- | --- |
| `LV_COLOR_DEPTH` | `16` | Color RGB565 |
| `LV_COLOR_16_SWAP` | `0` | Orden de bytes usado por el controlador |
| `LV_TICK_CUSTOM` | `1` | Tiempo del sistema mediante `millis()` |
| `LV_USE_USER_DATA` | `1` | Datos asociados a objetos y eventos |
| `LV_USE_QRCODE` | `1` | Generación de códigos QR |
| `LV_USE_SNAPSHOT` | `1` | Capturas de pantalla |
| `LV_MEM_CUSTOM` | `1` | Asignación con `malloc`, `free` y `realloc` |

Conserva las fuentes y el resto de opciones de este archivo para que los módulos de LVGL y el sketch compartan la misma configuración.

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

Instala las dependencias:

```powershell
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.10 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib update-index
arduino-cli lib install "lvgl@8.3.6" "NimBLE-Arduino@2.5.0"
```

Compila desde la raíz del repositorio:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CPUFreq=240,FlashSize=16M,FlashMode=qio,PSRAM=opi,PartitionScheme=custom,USBMode=default,CDCOnBoot=cdc,UploadSpeed=921600,DebugLevel=none" --build-path build firmware/password_manager
```

El firmware se genera en `build/`.

## Aviso de privacidad

Antes de distribuir tu firmware, completa [privacy_notice.h](../firmware/password_manager/privacy_notice.h) siguiendo [PRIVACY.es.md](PRIVACY.es.md). El repositorio incluye un mensaje visible de aviso pendiente de configurar en ambos idiomas.
