[English](FEATURES.md) | [Espanol](FEATURES.es.md)

---

# KeyVault / Huskey Pocket - Especificacion Tecnica de Funcionalidades

Documento tecnico descriptivo de las capacidades, arquitectura y caracteristicas operativas del gestor de contrasenas offline basado en hardware Waveshare ESP32-S3-Touch-LCD-2.8.

---

## 1. Descripcion General y Arquitectura Hardware

El dispositivo opera como una boveda fisica offline para el almacenamiento, consulta e inyeccion de credenciales digitales sin conexion a internet ni servicios en la nube.

* Microcontrolador: Espressif ESP32-S3 (Dual-Core Xtensa LX7 a 240 MHz con memoria PSRAM y Flash integradas).
* Pantalla: Panel LCD TFT IPS de 2.8 pulgadas, resolucion 240x320 pixeles con controlador ST7789 via bus SPI de alta velocidad.
* Panel Tactil: Digitalizador capacitivo CST328 sobre bus I2C (Wire1), con soporte de gestos y respuesta continua.
* Reloj en Tiempo Real (RTC): Chip dedicado PCF85063 conectado via bus I2C (Wire). Requiere una batería externa de respaldo para conservar la fecha y la hora al apagar el dispositivo y mantener el funcionamiento de TOTP entre apagados.
* Batería y carcasa: Batería principal LiPo de 3.7 V, formato **523450**. La carcasa está diseñada para alojar este formato; el respaldo del RTC utiliza una batería externa independiente.
* Sensor Inercial (IMU): Acelerometro y giroscopio de 6 ejes QMI8658C via I2C (Wire) para deteccion de movimiento y eventos dinamicos.
* Almacenamiento Extraible: Lector de tarjeta MicroSD operando en modo SD_MMC de 1 bit.
* Respuesta Haptica: Motor de vibracion ERM con circuito de excitacion por transistor para retroalimentacion tactil en pulsaciones y eventos de sistema.
* Gestion de Alimentacion: Circuito de lectura de voltaje de bateria mediante ADC calibrado, control de apagado de perifericos por transistor MOSFET de canal P (AO3401) y compatibilidad con modos de suspension profunda (Deep Sleep).

---

## 2. Seguridad y Boveda Criptografica

* Algoritmo de Cifrado: AES-256-CBC implementado con mbedTLS. CBC no proporciona cifrado autenticado.
* Derivacion de Claves: Derivacion de clave maestra a partir del PIN del usuario empleando PBKDF2-HMAC-SHA256 con 50,000 iteraciones y sal (salt) criptografica unica por dispositivo. Los respaldos `.hky` usan 100,000 iteraciones con una contrasena independiente.
* Autenticacion por PIN: Bloqueo de acceso mediante codigo numerico de 6 digitos (`PIN_DIGITS` en `vault.h`) con validacion en tiempo constante para mitigar ataques de temporizacion.
* Intentos de PIN: Contador persistente en NVS. El cuarto fallo muestra una advertencia; el quinto fallo consecutivo restablece el dispositivo y elimina la boveda.
* PIN Senuelo: Abre una boveda vacia sin acceso a los registros reales. Sus guardados son simulados y activa un temporizador configurable de restablecimiento que elimina la boveda.
* Factory Reset Protection (FRP): Tras el restablecimiento se solicita el PIN anterior. Tres fallos en esta comprobacion activan el bloqueo permanente del firmware.
* Almacenamiento Local Seguro: Las entradas cifradas residen en el sistema de archivos flash interno LittleFS y registros de configuracion en NVS (Non-Volatile Storage).
* Borrado Seguro (Secure Wipe): El restablecimiento elimina credenciales y material de claves. La nivelacion de desgaste de Flash puede conservar copias fisicas; el borrado no garantiza la imposibilidad de recuperacion en laboratorio.

---

## 3. Mecanismos de Proteccion Activa

* Bloqueo por Arrebato (Snatch Lock):
  * Monitorizacion continua de los vectores de aceleracion y velocidad angular mediante el sensor IMU QMI8658C.
  * Si se detecta un cambio brusco coincidente en aceleracion y rotacion (patron de tiron o arrebato violento de la mano), el firmware llama a la rutina de bloqueo de la boveda y entra en suspension profunda (Deep Sleep). La latencia total debe medirse en el hardware de destino.
  * Sensibilidad configurable en niveles: Desactivado, Bajo, Medio y Alto.
* Interruptor de Hombre Muerto (Dead Man's Switch):
  * Plazos de 7, 14, 30, 60, 90 o 180 dias; tambien puede desactivarse.
  * Al arrancar, si se ha cumplido el plazo sin un desbloqueo con el PIN real, se ejecuta un restablecimiento de fabrica que elimina la boveda. Los accesos con el PIN senuelo no renuevan el plazo.
* Bloqueo Automatico por Inactividad: Ajustes de 30 segundos, 1, 2 o 5 minutos, o desactivado. El temporizador apaga la pantalla y bloquea el acceso.

---

## 4. Emulacion de Teclado HID (Inyeccion de Credenciales)

El sistema permite escribir contraseñas y datos directamente en cualquier ordenador o dispositivo receptor simulando pulsaciones de teclado fisico sin requerir software ni drivers adicionales.

* Modo Teclado USB HID:
  * Utiliza el stack nativo TinyUSB del ESP32-S3 para enumerarse como un dispositivo estandar Human Interface Device (HID).
* Modo Teclado Bluetooth Low Energy (BLE):
  * Emulacion de teclado inalambrico mediante la libreria NimBLE-Arduino.
  * Compatibilidad con sistemas operativos Windows, macOS, Linux, Android e iOS.
  * Soporte de emparejamiento persistente y enlace cifrado (bonding).
* Disposiciones de Teclado (Layouts):
  * Soporte integrado para distribuciones ES-LAT (Espanol Latinoamerica), ES-ES (Espanol Espana) y US (Ingles Estados Unidos).
  * Mapeo automatico de caracteres especiales, simbolos ASCII extendidos, modificadores de tecla y mayusculas.
* Acciones de Inyeccion Rapida:
  * Boton de envio de Nombre de Usuario seguido de pulsacion automatica de la tecla Tabulador.
  * Boton de envio de Contrasena seguido de pulsacion automatica de la tecla Enter.
  * Inyeccion directa de claves TOTP generadas.
  * Inyeccion de contrasenas generadas al instante desde el generador integrado.

---

## 5. Gestion de Entradas y Credenciales

* Tipos de Registros Admitidos:
  * Cuentas Web y Servicios: Almacena Titulo, Nombre de Usuario, Contrasena, Notas/URL y Clave Secreta TOTP.
  * Secretos y Notas Seguras: Registros de texto libre de longitud ampliada para claves privadas, frases de recuperacion (seed phrases) o notas confidenciales.
* Estructura Jerarquica:
  * Organizacion en Categorias y Carpetas personalizables.
  * Posibilidad de crear, renombrar y eliminar carpetas (con opcion de reubicar o conservar las entradas contenidas).
  * Categoria automatica para elementos sin clasificar.
* Gestion de Favoritos:
  * Marcado directo de registros con atributo de favorito para rapido acceso desde la pantalla de inicio o vista filtrada.
* Motor de Busqueda Integrado:
  * Campo de busqueda en tiempo real con filtrado progresivo por nombre de servicio o usuario.
  * Control rapido de limpieza de consulta.
* Modos de Vista:
  * Lista de entradas o cuadricula de iconos con tres columnas.
  * Preferencia de vista guardada entre sesiones.
* Filtros y Orden:
  * Filtros por carpeta/categoria y favoritos; busqueda por nombre o usuario.
  * Nombre A-Z, nombre Z-A, fecha de creacion mas reciente o mas antigua.
  * Preferencia de orden persistida en NVS.
* Seleccion Multiple:
  * Seleccion de varias entradas, seleccionar todas las entradas de la vista y eliminacion por lotes.
* Enmascaramiento de Contrasenas:
  * Visualizacion ofuscada de campos confidenciales con opcion de alternancia a texto claro mediante boton tactil y confirmacion haptica.
* Exportacion Visual por Codigo QR:
  * Generacion dinamica de codigos QR monocromaticos en pantalla para transferir credenciales hacia dispositivos moviles sin contacto fisico ni uso de redes de comunicaciones.

---

## 6. Autenticacion de Dos Factores (TOTP / 2FA)

* Estandar Compatible: Algoritmo Time-based One-Time Password conforme a la norma RFC 6238 (HMAC-SHA1, codigos de 6 digitos, intervalo de validez de 30 segundos).
* Independencia de Red:
  * El calculo temporal se apoya exclusivamente en el reloj de hardware interno y RTC externo PCF85063.
  * Requiere una batería externa de respaldo para el RTC que conserve la fecha y la hora cuando el dispositivo está apagado.
* Interfaz Visual Dinamica:
  * Temporizador grafico y numerico con actualizacion segundo a segundo del tiempo restante antes de la caducidad del token.
* Escritura Automatizada:
  * Boton dedicado para tipear el token numerico actual directamente hacia el host mediante la conexion USB o Bluetooth activa.

---

## 7. Generador de Contrasenas Integrado

* Control de Longitud: Parametrizable de forma interactiva entre 8 y 32 caracteres mediante barra deslizante.
* Conjuntos de Simbolos Seleccionables:
  * Letras mayusculas (A-Z).
  * Letras minusculas (a-z) como conjunto base siempre habilitado.
  * Digitos numericos (0-9).
  * Simbolos y caracteres especiales puntuados.
* Exclusion de Ambiguedad: Filtro opcional para eliminar caracteres visualmente confusos (como 1, l, I, 0, O).
* Modalidad de Uso:
  * Pantalla de generacion independiente.
  * Generador integrado dentro de los formularios de alta y modificacion de cuentas.

---

## 8. Teclados Virtuales en Pantalla

El dispositivo incorpora dos esquemas de teclado tactil nativo disenados para operar con fluidez sobre paneles de 2.8 pulgadas:

* Teclado QWERTY Dividido (Split Keyboard):
  * Arquitectura basada en 6 paneles deslizantes (slides): Letras Izquierda, Letras Derecha, Simbolos 1 Izquierda, Simbolos 1 Derecha, Simbolos 2 Izquierda y Simbolos 2 Derecha.
  * Soporte de tecla Shift para una sola mayuscula y doble toque para bloqueo permanente (Caps Lock).
  * Teclas dedicadas de navegacion, borrado y aceptacion.
* Teclado Telefonico T9 (Grid 4x4):
  * Distribucion matricial de 16 teclas con pulsacion multiple (multitap) para seleccion secuencial de letras.
  * Deteccion de pulsacion prolongada (long press) para escritura directa de digitos numericos o cadenas frecuentes (como .com).
  * Selector de tipo de teclado QWERTY/T9 disponible en el onboarding y en Ajustes.

---

## 9. Almacenamiento, Respaldo y Transferencia de Datos

* Copias de Seguridad Cifradas (.hky - Huskey Backup):
  * Exportacion completa de la boveda hacia la tarjeta MicroSD protegida con contrasena independiente y cifrado PBKDF2 + AES-256.
* Exportacion en Texto Estructurado (CSV):
  * Opcion de generar un archivo CSV estandar para migracion hacia plataformas de escritorio, sujeto a confirmacion y advertencia de seguridad en pantalla.
* Proceso de Importacion No Bloqueante:
  * Mecanismo de procesamiento asincrono por bloques temporizados para evitar congelamiento de la interfaz grafica durante la lectura e importacion de archivos extensos.
* Destruccion Segura de Archivos Residuales:
  * Funcion de purga y sobreescritura de los directorios de exportacion en la tarjeta de memoria.
* Modo Memoria USB (USB Mass Storage):
  * Permite exponer el lector MicroSD como una unidad de disco extraible estandar ante la computadora para transferencia comoda de respaldos sin extraer la tarjeta fisica.

---

## 10. Interfaz, onboarding y personalización

### Asistente de configuración inicial

El onboarding presenta las siguientes pantallas:

| Pantalla | Opciones o función |
| --- | --- |
| Idioma | Español o inglés |
| Bienvenida | Introducción al dispositivo |
| Aviso de privacidad | Texto de la implementación y confirmación de lectura |
| Fecha | Día, mes y año |
| Hora | Horas, minutos y formato de 12 o 24 horas |
| Teclado | Idioma de entrada y tipo QWERTY o T9 |
| Bloqueo automático | 30 segundos, 1, 2 o 5 minutos, o desactivado |
| Tema | Selección de apariencia |
| Crear PIN | PIN maestro de 6 dígitos |
| Confirmar PIN | Verificación del PIN elegido |
| Preparación | Progreso de creación del almacenamiento de la bóveda |
| Finalización | Cierre de la configuración inicial |

### Fecha, hora y zona horaria

- Ajuste manual de día, mes y año desde la pantalla de fecha en Ajustes.
- Ajuste de horas y minutos y selección de formato de 12 o 24 horas.
- Reloj PCF85063 y visualización de la hora en la barra de estado.
- Batería externa de respaldo para el RTC, necesaria para conservar la fecha y la hora entre apagados y mantener el funcionamiento de TOTP.
- Desplazamiento horario de UTC−12 a UTC+14 en pasos de 30 minutos.
- Uso de la hora del dispositivo para TOTP y del tiempo transcurrido para el Modo perdido.

### Personalización

- Temas claro, oscuro y personalizado.
- Base clara u oscura para el tema personalizado.
- Edición independiente de fondo, tarjetas y acento mediante tono y saturación.
- Colores de acento predefinidos: azul, verde, naranja, rosa, morado y celeste.
- Vista previa de la paleta durante la edición y ajuste del color de texto según el fondo.
- Idioma de la interfaz en español o inglés, guardado en las preferencias del dispositivo.

### Controles e indicadores

- Barra de estado con reloj, nivel de batería y animación de carga.
- Avisos flotantes para resultados, errores y confirmaciones.
- Respuesta háptica en las acciones de la interfaz.
- Menú de energía para bloquear, reiniciar o apagar.
- Selector de teclado táctil QWERTY/T9 e idioma de entrada desde Ajustes.

## 11. Aviso de privacidad por implementacion

Cada implementador debe redactar, publicar y mantener su propio aviso. Los textos ES/EN de `firmware/password_manager/privacy_notice.h` se comparten entre el asistente y Ajustes. El mensaje predeterminado indica que el aviso esta pendiente de configurar; consulta [PRIVACY.es.md](docs/PRIVACY.es.md).

## 12. Librerias y herramientas

| Componente | Version | Funcion |
| --- | --- | --- |
| Arduino-ESP32 (`esp32:esp32`) | 3.3.10 | Nucleo de la placa, compilador y APIs ESP-IDF |
| LVGL (`lvgl`) | 8.3.6 | Interfaz, controles, QR y capturas |
| NimBLE-Arduino | 2.5.0 | Teclado BLE HID y emparejamiento |

Arduino-ESP32 proporciona `SPI`, `Wire`, `FS`, `LittleFS`, `SD_MMC`, `Preferences`, `USB`, `USBHID`, `USBMSC`, mbedTLS y FreeRTOS. Los controladores ST7789, CST328, PCF85063 y QMI8658C se compilan desde los archivos del sketch.

`lv_conf.h` activa color de 16 bits, tick basado en `millis()`, QR, snapshots, datos de usuario y fuentes de la interfaz. Consulta la [lista e instalacion de dependencias](docs/BUILD.es.md) para los pasos y opciones de compilacion.

## 13. Batería, energía y capturas

| Función | Comportamiento |
| --- | --- |
| Medición | Lectura ADC del voltaje de la LiPo con calibración y promedio de muestras |
| Nivel | Estimación a partir del voltaje y representación en pasos de 25 % en el icono de batería |
| Carga | Estimación por tendencia de voltaje y animación del segmento de carga |
| Batería baja | Cambio de color del indicador al 15 % o menos, cuando no está cargando |
| Estabilidad de lectura | Filtrado rápido/lento y compensación de transiciones de carga |
| Energía | Bloqueo, reinicio, apagado, control de retroiluminación y suspensión profunda |

Las capturas de pantalla se guardan como BMP en `/export/` de la microSD. Los comandos serie son `shot` para tamaño original, `shot2` para escala 2× y `shot3` para escala 3×.

## 14. Restablecimiento de fábrica y FRP

- El restablecimiento manual se inicia desde **Ajustes → Restablecer fábrica**.
- Requiere confirmar la operación, escribir exactamente **`Reset`** y mantener pulsado el control durante **5 segundos**.
- Borra entradas de la bóveda, PIN de acceso, carpetas/categorías, preferencias y configuración del señuelo.
- La verificación FRP permanece almacenada después del restablecimiento.
- Antes de configurar de nuevo el dispositivo, la pantalla FRP solicita el PIN real registrado anteriormente.
- El contador FRP es persistente. Al tercer intento incorrecto se activa el bloqueo permanente del firmware.

## 15. PIN señuelo

- Se configura con un PIN distinto del PIN real.
- Al introducirlo, se muestra una bóveda vacía y los guardados de la sesión son simulados.
- Los registros reales no se cargan en la sesión señuelo.
- El temporizador de restablecimiento tiene opciones de **2, 5 o 10 minutos**, con 5 minutos como valor predeterminado.
- Al agotarse el temporizador se restablece el dispositivo y se elimina la bóveda; la protección FRP sigue activa.
- La sesión real permite activar, cambiar o desactivar el PIN señuelo y ajustar su temporizador desde Ajustes.
