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
* Reloj en Tiempo Real (RTC): Chip dedicado PCF85063 conectado via bus I2C (Wire) con respaldo por bateria para mantener la estampa temporal independiente del microcontrolador.
* Sensor Inercial (IMU): Acelerometro y giroscopio de 6 ejes QMI8658C via I2C (Wire) para deteccion de movimiento y eventos dinamicos.
* Almacenamiento Extraible: Lector de tarjeta MicroSD operando en modo SD_MMC de 1 bit.
* Respuesta Haptica: Motor de vibracion ERM con circuito de excitacion por transistor para retroalimentacion tactil en pulsaciones y eventos de sistema.
* Gestion de Alimentacion: Circuito de lectura de voltaje de bateria mediante ADC calibrado, control de apagado de perifericos por transistor MOSFET de canal P (AO3401) y compatibilidad con modos de suspension profunda (Deep Sleep).

---

## 2. Seguridad y Boveda Criptografica

* Algoritmo de Cifrado: AES-256-CBC implementado con mbedTLS. CBC no proporciona cifrado autenticado.
* Derivacion de Claves: Derivacion de clave maestra a partir del PIN del usuario empleando PBKDF2-HMAC-SHA256 con 50,000 iteraciones y sal (salt) criptografica unica por dispositivo. Los respaldos `.hky` usan 100,000 iteraciones con una contrasena independiente.
* Autenticacion por PIN: Bloqueo de acceso mediante codigo numerico de 6 digitos (`PIN_DIGITS` en `vault.h`) con validacion en tiempo constante para mitigar ataques de temporizacion.
* Proteccion contra Fuerza Bruta: Sistema de penalizacion exponencial y bloqueo progresivo ante intentos fallidos de introduccion del PIN.
* Almacenamiento Local Seguro: Las entradas cifradas residen en el sistema de archivos flash interno LittleFS y registros de configuracion en NVS (Non-Volatile Storage).
* Borrado Seguro (Secure Wipe): El restablecimiento elimina credenciales y material de claves. La nivelacion de desgaste de Flash puede conservar copias fisicas; el borrado no garantiza la imposibilidad de recuperacion en laboratorio.

---

## 3. Mecanismos de Proteccion Activa

* Bloqueo por Arrebato (Snatch Lock):
  * Monitorizacion continua de los vectores de aceleracion y velocidad angular mediante el sensor IMU QMI8658C.
  * Si se detecta un cambio brusco coincidente en aceleracion y rotacion (patron de tiron o arrebato violento de la mano), el firmware llama a la rutina de bloqueo de la boveda y entra en suspension profunda (Deep Sleep). La latencia total debe medirse en el hardware de destino.
  * Sensibilidad configurable en niveles: Desactivado, Bajo, Medio y Alto.
* Interruptor de Hombre Muerto (Dead Man's Switch):
  * Temporizador de seguridad parametrizable (dias, semanas o meses).
  * En caso de no registrarse un desbloqueo autenticado dentro del plazo estipulado, el dispositivo ejecuta acciones defensivas automatizadas (bloqueo permanente o formateo seguro de credenciales).
* Bloqueo Automatico por Inactividad: Temporizador de reposo ajustable que apaga la pantalla y bloquea el acceso si no se detecta contacto tactil durante un lapso predeterminado.

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
* Enmascaramiento de Contrasenas:
  * Visualizacion ofuscada de campos confidenciales con opcion de alternancia a texto claro mediante boton tactil y confirmacion haptica.
* Exportacion Visual por Codigo QR:
  * Generacion dinamica de codigos QR monocromaticos en pantalla para transferir credenciales hacia dispositivos moviles sin contacto fisico ni uso de redes de comunicaciones.

---

## 6. Autenticacion de Dos Factores (TOTP / 2FA)

* Estandar Compatible: Algoritmo Time-based One-Time Password conforme a la norma RFC 6238 (HMAC-SHA1, codigos de 6 digitos, intervalo de validez de 30 segundos).
* Independencia de Red:
  * El calculo temporal se apoya exclusivamente en el reloj de hardware interno y RTC externo PCF85063.
* Interfaz Visual Dinamica:
  * Temporizador grafico y numerico con actualizacion segundo a segundo del tiempo restante antes de la caducidad del token.
* Escritura Automatizada:
  * Boton dedicado para tipear el token numerico actual directamente hacia el host mediante la conexion USB o Bluetooth activa.

---

## 7. Generador de Contrasenas Integrado

* Control de Longitud: Parametrizable de forma interactiva entre 8 y 32 caracteres mediante barra deslizante.
* Conjuntos de Simbolos Seleccionables:
  * Letras mayusculas (A-Z).
  * Letras minusculas (a-z).
  * Digitos numericos (0-9).
  * Simbolos y caracteres especiales puntuados.
* Exclusion de Ambiguedad: Filtro opcional para eliminar caracteres visualmente confusos (como 1, l, I, 0, O).
* Modalidad de Uso:
  * Pantalla de generacion independiente.
  * Generador integrado dentro de los formularios de alta y modificacion de cuentas.

---

## 8. Teclados Virtuales en Pantalla

El dispositivo incorpora dos esquemas de teclado tactil nativo disenados para operar con fluidez sobre paneles de 2.8 pulgadas:

* Teclado Dividido KeyVault (Split Keyboard):
  * Arquitectura basada en 6 paneles deslizantes (slides): Letras Izquierda, Letras Derecha, Simbolos 1 Izquierda, Simbolos 1 Derecha, Simbolos 2 Izquierda y Simbolos 2 Derecha.
  * Soporte de tecla Shift para una sola mayuscula y doble toque para bloqueo permanente (Caps Lock).
  * Teclas dedicadas de navegacion, borrado y aceptacion.
* Teclado Telefonico T9 (Grid 4x4):
  * Distribucion matricial de 16 teclas con pulsacion multiple (multitap) para seleccion secuencial de letras.
  * Deteccion de pulsacion prolongada (long press) para escritura directa de digitos numericos o cadenas frecuentes (como .com).
  * Selector agil de esquema de teclado disponible desde la configuracion de usuario.

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

## 10. Interfaz de Usuario y Experiencia (LVGL 8.x)

* Barra de Estado (Status Bar):
  * Monitor de nivel de bateria discretizado por cuartos con indicador de carga activa.
  * Visualizacion horaria continua sincronizada con el RTC.
  * Iconografia de estado de almacenamiento MicroSD y bloqueo.
* Sistema de Tematizacion:
  * Modo Oscuro optimizado para bajo consumo y entornos con poca iluminacion.
  * Modo Claro para visualizacion con luz ambiental intensa.
  * Modo Personalizado con ajuste independiente de tono (Hue) y saturacion para fondo, tarjetas y acentos.
  * Evaluacion algoritmica de luminancia perceptual para asegurar contraste legibilidad de textos en cualquier combinacion de color.
* Notificaciones Flotantes (Toasts):
  * Mensajes emergentes de diseno sobrio para confirmacion de operaciones (guardado, eliminacion, avisos de error) en capas superiores de la interfaz.
* Menu de Control de Energia:
  * Panel emergente con accesos rapidos para Apagar, Reiniciar o Bloquear el equipo.
* Asistente de Configuracion Inicial (Onboarding):
  * Procedimiento guiado para el primer arranque que asiste al usuario en la definicion de su PIN maestro, seleccion de idioma, ajuste de fecha/hora y preferencias base.
* Arquitectura Multilenguaje:
  * Sistema de cadenas i18n estructurado para soporte completo de idiomas sin dependencia de red.

## 11. Aviso de privacidad por implementacion

Cada implementador debe redactar, publicar y mantener su propio aviso. Los textos ES/EN de `firmware/password_manager/privacy_notice.h` se comparten entre el asistente y Ajustes. El mensaje predeterminado indica que el aviso esta pendiente de configurar; consulta [PRIVACY.es.md](docs/PRIVACY.es.md).
