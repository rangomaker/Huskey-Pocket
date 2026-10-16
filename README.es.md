[English](README.md) | **Español**

# Huskey Pocket

Gestor de contraseñas físico basado en **Waveshare ESP32-S3-Touch-LCD-2.8**. Guarda credenciales localmente, genera códigos TOTP y permite escribir datos mediante teclado USB o Bluetooth Low Energy, sin depender de cuentas ni servicios en la nube.

**Dispositivo construido y funcionando**, con las piezas impresas y el encaje de la carcasa probados físicamente.

<p align="center">
  <img src="assets/photos/huskey-entries-light.jpg" width="320" alt="Huskey Pocket real con la lista de entradas en tema claro">
  <img src="assets/photos/huskey-entries-dark.jpg" width="320" alt="Huskey Pocket real con la lista de entradas en tema oscuro">
</p>

[Fotos del dispositivo real, renders y pantallas](docs/GALLERY.md).

> **Proyecto de referencia, sin mantenimiento activo.** Se comparte bajo licencia MIT, tal cual, sin soporte técnico, correcciones ni nuevas funciones garantizadas. Puedes hacer un fork y continuar su desarrollo.

**Impresión 3D:** [3MF listo para Bambu Lab A1 · PETG · boquilla de 0.4 mm](hardware/Huskey_Pocket_A1_PETG_04.3mf) · [Configuración e instrucciones](hardware/bambu/README.md#español).

## Características

### Configuración inicial (onboarding)

- Pantallas de selección de idioma, bienvenida y aviso de privacidad.
- Configuración guiada de fecha, hora, idioma de entrada, teclado táctil, bloqueo automático y tema.
- Creación y confirmación del PIN maestro.
- Pantalla de progreso para preparar la bóveda y pantalla de finalización.

### Fecha y hora

- Ajuste manual de día, mes, año, horas y minutos desde el onboarding y Ajustes.
- Formato de **12 o 24 horas** y reloj visible en la barra de estado.
- Zona horaria configurable de **UTC−12 a UTC+14**, en pasos de 30 minutos.
- RTC **PCF85063** para mantener la hora y generar TOTP sin sincronización por internet. Las funciones de fecha, hora y TOTP requieren una **batería externa de respaldo para el RTC**, que conserva la fecha y la hora al apagar el dispositivo.

### Credenciales y organización

- Cuentas con título, usuario, contraseña, notas/URL y secreto TOTP; registros de secretos y notas.
- Creación y edición de entradas, carpetas y categorías; cambio de nombre y traslado de registros.
- Favoritos y campos sensibles ocultos con opción de mostrarlos.
- Transferencia visual de datos mediante QR.

### Vistas, búsqueda, filtros y orden

- Vista de entradas en **lista** o **cuadrícula de iconos de tres columnas**.
- Orden por **nombre A–Z**, **nombre Z–A**, **fecha reciente** o **fecha antigua**; el orden por fecha utiliza la creación de la entrada.
- Filtros por carpeta/categoría y favoritos, y búsqueda por nombre o usuario.
- Selección múltiple, selección de todas las entradas de la vista y eliminación por lotes.
- El modo de vista y el orden elegido se guardan en las preferencias.

### PIN y bóveda

- PIN maestro de **6 dígitos**, con cambio desde Ajustes y recifrado de las entradas.
- **AES-256-CBC**, IV aleatorio por operación y **PBKDF2-HMAC-SHA256 de 50 000 iteraciones**.
- Bóveda en LittleFS y configuración en NVS.
- Contador persistente de intentos: **cinco fallos consecutivos de PIN restablecen el dispositivo y borran la bóveda**.

### PIN señuelo

- PIN alternativo, distinto del real, que abre una bóveda vacía sin acceso a las credenciales reales.
- Los guardados en la sesión señuelo son simulados.
- Temporizador de **2, 5 o 10 minutos**, configurable en Ajustes, que restablece el dispositivo y elimina la bóveda al agotarse.
- Opciones para activar, cambiar o desactivar el PIN señuelo.

### Restablecimiento de fábrica y FRP

- Restablecimiento manual desde Ajustes, con aviso de confirmación, escritura de la palabra **`Reset`** y pulsación sostenida de **5 segundos**.
- Elimina las entradas, el PIN de acceso, las carpetas y las preferencias del dispositivo.
- **Factory Reset Protection (FRP)** conserva la verificación del PIN anterior y la solicita para volver a configurar el dispositivo.
- **Tres intentos FRP incorrectos** activan el bloqueo permanente del firmware.

### Bloqueo y protección física

- Bloqueo por inactividad a los **30 segundos, 1, 2 o 5 minutos**, o desactivado.
- **Snatch Lock**: detección de aceleración y giro con QMI8658C, seguida de bloqueo y suspensión profunda. Sensibilidad baja, media, alta o desactivada.
- **Modo perdido**: restablecimiento al arrancar si transcurren **7, 14, 30, 60, 90 o 180 días** sin un desbloqueo con el PIN real, según la configuración.

### Teclado USB y Bluetooth

- Teclado **USB HID** y teclado inalámbrico **Bluetooth Low Energy HID**.
- Distribuciones **ES-LAT, ES-ES y US**, con mapeo de letras, símbolos y modificadores.
- Envío de usuario seguido de Tab, contraseña seguida de Enter, códigos TOTP y contraseñas generadas.
- Confirmación del código de emparejamiento y gestión de dispositivos Bluetooth vinculados.

### Teclados táctiles T9 y QWERTY

- Selector de tipo de teclado en el onboarding y en Ajustes.
- **QWERTY deslizante**: dos páginas de letras y cuatro de símbolos, con Shift y bloqueo de mayúsculas.
- **T9 de 4 × 4**: teclado telefónico con pulsación múltiple para letras y pulsación prolongada para números y atajos.
- Controles para borrar, confirmar y cambiar de panel durante la escritura.

### Personalización de la interfaz

- Idiomas **español e inglés** y temas **claro, oscuro y personalizado**.
- Edición independiente del **fondo**, las **tarjetas** y el **color de acento**, con ajustes de tono y saturación.
- Base clara u oscura para el tema personalizado y selección de colores de acento predefinidos.
- Ajuste automático del color de texto según el fondo, avisos flotantes y respuesta háptica.

### TOTP y generador de contraseñas

- Códigos TOTP de **6 dígitos**, HMAC-SHA1 e intervalos de **30 segundos**, compatibles con RFC 6238, con cuenta atrás en pantalla.
- Generador de **8 a 32 caracteres**: minúsculas como conjunto base, mayúsculas, números y símbolos opcionales, y filtro de caracteres ambiguos.
- Generador independiente e integrado en los formularios, con envío directo al equipo conectado.

### Respaldos y transferencia

- Respaldos **`.hky` cifrados**, con contraseña independiente del PIN y PBKDF2 de **100 000 iteraciones**.
- Importación y exportación **CSV**; el CSV contiene los datos en texto legible.
- Importación por bloques y limpieza de archivos exportados en la microSD.
- Modo **USB Mass Storage (MSC)** para acceder a la microSD, con opción de solo lectura.
- Modo de almacenamiento exclusivo: la aplicación y las funciones de teclado quedan suspendidas mientras está activo.

### Batería y energía

- La carcasa está diseñada para alojar una **batería LiPo de 3.7 V, formato 523450**. El RTC utiliza una batería externa de respaldo independiente de la batería principal.
- Lectura de voltaje de la LiPo y estimación del nivel de batería.
- Indicador de nivel en la barra de estado, animación de carga y señal de batería baja.
- Filtrado de lecturas y compensación de cambios de tensión al conectar o desconectar la carga.
- Menú para bloquear, reiniciar o apagar, control de retroiluminación y suspensión profunda.

### Capturas y recursos del proyecto

- Capturas **BMP en microSD** mediante los comandos serie `shot`, `shot2` y `shot3`.
- Código fuente, manuales, whitepaper, galería de renders, pantallas de UI, [carcasa editable en Blender](hardware/CASE.blend) y [cuatro piezas STL](hardware/STL/) para impresión 3D.
- [Proyecto 3MF para Bambu Lab A1](hardware/Huskey_Pocket_A1_PETG_04.3mf), preparado para **PETG y boquilla de 0.4 mm**: las cuatro piezas orientadas en una sola cama, con soportes y ajustes de impresión incluidos para **placa PEI texturizada**.

Consulta [FEATURES.es.md](FEATURES.es.md) para el detalle de cada módulo.

[Cambios de la versión 1.0.1](CHANGELOG.md).

## Empieza aquí

1. Consulta el [índice de documentación](docs/README.md) y el manual de usuario.
2. Abre [password_manager.ino](firmware/password_manager/password_manager.ino) en Arduino IDE. Conserva juntos todos los archivos del sketch.
3. Instala las dependencias y configura la placa con la [guía de compilación](docs/BUILD.es.md).
4. Personaliza el [aviso de privacidad](docs/PRIVACY.es.md) antes de distribuir tu implementación.
5. Imprime la carcasa con el [proyecto 3MF para A1](hardware/Huskey_Pocket_A1_PETG_04.3mf). Ábrelo como proyecto en Bambu Studio y pulsa **Laminar placa**; consulta la [guía de impresión](hardware/bambu/README.md#español) para los ajustes y la orientación de cada pieza.

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
| Batería principal | LiPo de 3.7 V, formato **523450**; carcasa diseñada para este formato |
| Respaldo del RTC | Batería externa para PCF85063, necesaria para conservar la fecha y la hora y mantener el funcionamiento de TOTP entre apagados |
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
assets/photos/              Fotografías del dispositivo real armado y funcionando
assets/ui/                  Referencias de interfaz en alta resolución
hardware/CASE.blend         Fuente Blender de la carcasa
hardware/STL/               Piezas para impresión 3D: BUTTON, COVER, FRAME y PLATE
hardware/*.3mf              Proyecto de impresión para A1, PETG y boquilla de 0.4 mm
hardware/bambu/             Guía de impresión y configuración en español e inglés
FEATURES.es.md              Funciones en español
FEATURES.md                 Funciones en inglés
LICENSE                     Licencia MIT
```

## Licencia

[MIT](LICENSE). Las dependencias y recursos de terceros conservan sus respectivas licencias; consulta [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
