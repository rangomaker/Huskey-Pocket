# Recursos de terceros / Third-party resources

La licencia MIT existente del proyecto se conserva en [LICENSE](LICENSE). Las dependencias y recursos de terceros mantienen sus propios avisos y condiciones.

The existing project MIT License is preserved in [LICENSE](LICENSE). Dependencies and third-party resources retain their own notices and terms.

| Recurso / Resource | Uso / Usage | Procedencia en esta copia / Source in this copy |
| --- | --- | --- |
| Arduino-ESP32 | Core, USB, SD_MMC, LittleFS, Preferences, mbedTLS | Dependencia instalada por separado / Separately installed dependency |
| LVGL 8.3.6 | Interfaz gráfica / User interface | Dependencia instalada por separado; `lv_conf.h` recuperado de la instalación local / Separately installed dependency; configuration recovered from the local installation |
| NimBLE-Arduino 2.5.0 | Bluetooth HID | Dependencia instalada por separado / Separately installed dependency |
| Inter | Fuentes `inter_*.c` / Fonts | Los encabezados de los archivos indican conversión desde TTF Inter / File headers identify the Inter TTF source |
| Material Symbols y otros glifos / and other glyphs | `material_16*.c` e iconos / and icons | Los encabezados de conversión identifican Material Symbols; se conserva el material recibido / Conversion headers identify Material Symbols; supplied resources are preserved |
| Waveshare | Plataforma y controladores de pantalla/táctil / Board and display/touch drivers | Archivos suministrados con el proyecto / Supplied project files |

Este inventario registra la procedencia identificable en los archivos recibidos; no sustituye los textos de licencia de cada recurso. Los TTF originales y sus licencias no estaban incluidos en la carpeta de recursos. Para redistribuir recursos de terceros, conserva sus avisos y verifica sus licencias originales. Los renders y el modelo Blender se incluyen tal como fueron suministrados.

This inventory records provenance identifiable in the supplied files; it does not replace each resource's license text. Original TTF files and their licenses were not included in the resource folder. Preserve notices and verify original licenses when redistributing third-party resources. Renders and the Blender model are included as supplied.
