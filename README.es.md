[English](README.md) | **Español**

# Huskey Pocket

Gestor de contraseñas físico basado en **Waveshare ESP32-S3-Touch-LCD-2.8**. Guarda credenciales localmente, genera códigos TOTP y permite escribir datos mediante teclado USB o Bluetooth Low Energy, sin depender de cuentas ni servicios en la nube.

<p align="center">
  <img src="assets/renders/huskey-gray-three-quarter.png" width="360" alt="Huskey Pocket gris">
  <img src="assets/renders/huskey-black-three-quarter.png" width="360" alt="Huskey Pocket negro">
</p>

> **Proyecto de referencia, sin mantenimiento activo.** Se comparte bajo licencia MIT, tal cual, sin soporte técnico, correcciones ni nuevas funciones garantizadas. Puedes hacer un fork y continuar su desarrollo.

## Características

### Credenciales y organización

- Cuentas con título, usuario, contraseña, notas/URL y secreto TOTP.
- Registros de secretos y notas para guardar códigos, claves y texto privado.
- Carpetas y categorías: creación, cambio de nombre, traslado de entradas y eliminación.
- Favoritos y búsqueda por nombre o usuario.
- Campos sensibles ocultos con opción de mostrarlos y transferencia visual mediante QR.

### PIN y bóveda

- PIN maestro de **6 dígitos**, con cambio de PIN desde Ajustes y recifrado de las entradas.
- Cifrado **AES-256-CBC** con IV aleatorio por operación y derivación **PBKDF2-HMAC-SHA256 de 50 000 iteraciones**.
- Bóveda en LittleFS y preferencias del dispositivo en NVS.
- Contador persistente de intentos fallidos; **cinco fallos consecutivos de PIN restablecen el dispositivo y borran la bóveda**.
- PIN señuelo para abrir una sesión sin acceso a las credenciales reales, con guardados simulados y un temporizador configurable que restablece el dispositivo y borra la bóveda.
- Protección de restablecimiento de fábrica (FRP) vinculada al PIN anterior.

### Bloqueo y protección física

- Bloqueo por inactividad a los **30 segundos, 1, 2 o 5 minutos**, o desactivado.
- **Snatch Lock**: detección de aceleración y giro mediante QMI8658C, seguida de bloqueo y suspensión profunda. Sensibilidad baja, media, alta o desactivada.
- **Modo perdido**: restablecimiento al arrancar si transcurren **7, 14, 30, 60, 90 o 180 días** sin un desbloqueo con el PIN real, según la configuración.

### Teclado USB y Bluetooth

- Teclado **USB HID** y teclado inalámbrico **Bluetooth Low Energy HID**.
- Distribuciones **ES-LAT, ES-ES y US**, con mapeo de letras, símbolos y modificadores.
- Envío de usuario seguido de Tab, contraseña seguida de Enter, códigos TOTP y contraseñas generadas.
- Confirmación del código de emparejamiento Bluetooth y gestión de dispositivos vinculados desde Ajustes.

### TOTP y generador de contraseñas

- Códigos TOTP de **6 dígitos**, HMAC-SHA1 e intervalos de **30 segundos**, compatibles con RFC 6238.
- Reloj **PCF85063** para mantener la hora sin sincronización por internet; cuenta atrás de validez en pantalla.
- Generador de **8 a 32 caracteres** con minúsculas, opciones de mayúsculas, números y símbolos, y filtro de caracteres ambiguos.
- Generador independiente e integrado en los formularios, con envío directo al equipo conectado.

### Respaldos y transferencia

- Exportación e importación de respaldos **`.hky` cifrados**, con contraseña independiente del PIN y PBKDF2 de **100 000 iteraciones**.
- Importación y exportación **CSV** para migrar registros. El CSV contiene los datos en texto legible.
- Importación por bloques para mantener la respuesta de la interfaz y limpieza de archivos exportados en la microSD.
- Modo **USB Mass Storage (MSC)** para acceder a la microSD desde el ordenador, con opción de solo lectura.
- Modo de almacenamiento exclusivo: al activarlo, la aplicación y las funciones de teclado quedan suspendidas.

### Interfaz táctil

- Idiomas **español e inglés** y temas **claro, oscuro y personalizado**.
- Ajuste de tono y saturación para fondo, tarjetas y acentos.
- Teclado dividido en **6 paneles**, con Shift y bloqueo de mayúsculas, y teclado **T9 de 4 × 4** con pulsación múltiple y prolongada.
- Asistente inicial para idioma, aviso de privacidad, fecha y hora, distribución de teclado, bloqueo, tema y PIN.
- Barra de estado con hora, batería, carga y microSD; avisos flotantes y respuesta háptica.

### Energía y capturas

- Menú para bloquear, reiniciar o apagar, control de retroiluminación y suspensión profunda.
- Capturas de pantalla **BMP en microSD** mediante los comandos serie `shot`, `shot2` y `shot3`.
- Código fuente, manuales, whitepaper, galería de renders, pantallas de UI y carcasa editable en Blender.

Consulta [FEATURES.es.md](FEATURES.es.md) para el detalle de cada módulo.

## Empieza aquí

1. Consulta el [índice de documentación](docs/README.md) y el manual de usuario.
2. Abre [password_manager.ino](firmware/password_manager/password_manager.ino) en Arduino IDE. Conserva juntos todos los archivos del sketch.
3. Instala las dependencias y configura la placa con la [guía de compilación](docs/BUILD.es.md).
4. Personaliza el [aviso de privacidad](docs/PRIVACY.es.md) antes de distribuir tu implementación.

## Aviso de privacidad

**Cada persona u organización que adapte, implemente o distribuya Huskey Pocket debe redactar, publicar y mantener su propio aviso de privacidad.** Los autores del repositorio no asumen esa gestión por terceros.

[privacy_notice.h](firmware/password_manager/privacy_notice.h) concentra el texto en español e inglés que muestran el asistente inicial y Ajustes. Por defecto indica **«Aviso pendiente de configurar»**; la casilla confirma su lectura. Sustituye el contenido por el aviso de tu implementación y actualiza su versión. La licencia MIT del código se conserva en [LICENSE](LICENSE).

## Hardware y compilación

| Elemento | Configuración del proyecto |
| --- | --- |
| Microcontrolador | ESP32-S3, 240 MHz |
| Flash | **16 MB** |
| PSRAM | OPI PSRAM, 8 MB |
| Pantalla / táctil | ST7789, 240 × 320 / CST328 |
| RTC / IMU | PCF85063 / QMI8658C |
| Almacenamiento externo | MicroSD, SD_MMC de 1 bit |
| Batería | LiPo de 3.7 V para uso portátil |
| Respuesta háptica | Motor de vibración |
| USB Mode | **USB-OTG (TinyUSB)** para HID y MSC |

## Librerías necesarias

### Paquete de la placa

| Paquete | Versión | Instalación |
| --- | --- | --- |
| [Arduino-ESP32 de Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html), `esp32:esp32` | **3.3.10** | Gestor de placas de Arduino IDE |

### Librerías externas

Instálalas desde el **Gestor de bibliotecas** de Arduino IDE seleccionando estas versiones:

| Nombre en el gestor | Versión | Función |
| --- | --- | --- |
| [lvgl](https://github.com/lvgl/lvgl/tree/v8.3.6) | **8.3.6** | Interfaz táctil, widgets, temas, QR y capturas de pantalla |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino/releases/tag/2.5.0) | **2.5.0** | Teclado Bluetooth Low Energy, emparejamiento y dispositivos vinculados |

### Componentes del paquete ESP32

| Componente | Función |
| --- | --- |
| `SPI` y `Wire` | Buses de pantalla, táctil, RTC e IMU |
| `FS`, `LittleFS` y `SD_MMC` | Bóveda interna y archivos en microSD |
| `Preferences` | Configuración y contadores en NVS |
| `USB`, `USBHID` y `USBMSC` | Teclado USB y almacenamiento masivo |
| `mbedTLS` | AES, SHA y derivación PBKDF2 |
| `FreeRTOS` y APIs ESP-IDF | Tareas, suspensión, GPIO y funciones del ESP32-S3 |

Estos componentes se instalan con **Arduino-ESP32 3.3.10**. Los controladores ST7789, CST328, PCF85063 y QMI8658C, las fuentes y los teclados táctiles están en `firmware/password_manager/`.

Mantén [lv_conf.h](firmware/password_manager/lv_conf.h) junto al sketch: configura color de 16 bits, tick de tiempo, QR, snapshots y fuentes. La [guía de compilación](docs/BUILD.es.md) incluye la instalación paso a paso y todos los ajustes de la placa.

## Interfaz

<p align="center">
  <img src="assets/ui/list-dark.png" width="180" alt="Lista en tema oscuro">
  <img src="assets/ui/list-light.png" width="180" alt="Lista en tema claro">
  <img src="assets/ui/generator-dark.png" width="180" alt="Generador de contraseñas">
  <img src="assets/ui/totp-setup-dark.png" width="180" alt="Configuración TOTP">
</p>

[Ver la galería completa](docs/GALLERY.md).

## Estructura

```text
firmware/password_manager/   Sketch Arduino, módulos, fuentes y lv_conf.h
docs/                       Guías e índice
docs/reference/             Manuales y whitepaper (DOCX/PDF)
assets/renders/             Renders del dispositivo
assets/ui/                  Referencias de interfaz en alta resolución
hardware/CASE.blend         Fuente Blender de la carcasa
FEATURES.es.md              Funciones en español
FEATURES.md                 Funciones en inglés
LICENSE                     Licencia MIT
```

## Licencia

[MIT](LICENSE). Las dependencias y recursos de terceros conservan sus respectivas licencias; consulta [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
