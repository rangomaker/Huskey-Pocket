[English](README.md) | **Español**

# Huskey Pocket

Gestor de contraseñas físico basado en **Waveshare ESP32-S3-Touch-LCD-2.8**. Guarda credenciales localmente, genera códigos TOTP y permite escribir datos mediante teclado USB o Bluetooth Low Energy, sin depender de cuentas ni servicios en la nube.

<p align="center">
  <img src="assets/renders/huskey-gray-three-quarter.png" width="360" alt="Huskey Pocket gris">
  <img src="assets/renders/huskey-black-three-quarter.png" width="360" alt="Huskey Pocket negro">
</p>

> **Proyecto de referencia, sin mantenimiento activo.** Se comparte bajo licencia MIT, tal cual, sin soporte técnico, correcciones ni nuevas funciones garantizadas. Puedes hacer un fork y continuar su desarrollo.

## Funciones y recursos

- Bóveda AES-256-CBC y derivación PBKDF2-HMAC-SHA256 del PIN.
- PIN maestro de 6 dígitos en esta versión, bloqueo por inactividad, PIN señuelo y protecciones ante intentos fallidos.
- Teclados USB HID y BLE con distribuciones ES-LAT, ES-ES y US.
- TOTP, generador de contraseñas, notas, carpetas, favoritos y visualización mediante QR.
- Respaldos cifrados `.hky`, importación/exportación CSV y modo USB MSC para microSD.
- Snatch Lock mediante IMU y modo perdido con borrado por inactividad prolongada.
- Interfaz en español e inglés, temas claro/oscuro/personalizado y teclados dividido y T9.
- Código fuente, manuales, whitepaper, renders, referencias de UI y carcasa en Blender.

Consulta [FEATURES.es.md](FEATURES.es.md) para conocer todas las funciones.

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
| Dependencias | Arduino-ESP32 3.3.10, LVGL 8.3.6, NimBLE-Arduino 2.5.0 |
| USB Mode | **USB-OTG (TinyUSB)** para HID y MSC |

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
