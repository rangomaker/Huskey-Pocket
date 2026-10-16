# Verificación / Validation

Fecha / Date: 2026-10-16.

## Integridad del repositorio / Repository integrity

- Se cotejaron las huellas SHA-256 de los recursos antes y después de organizarlos. / Resource SHA-256 hashes were compared before and after organization.
- Se conservan sin cambios seis manuales/documentos, trece renders, veinticuatro referencias de UI y `CASE.blend`. / Six manuals/documents, thirteen renders, twenty-four UI references and `CASE.blend` are preserved unchanged.
- Los cambios en archivos originales se limitan a README/FEATURES en ambos idiomas, `.gitignore`, traducciones, aviso en Ajustes y asistente inicial. / Original-file changes are limited to both README/FEATURES languages, `.gitignore`, translations, the Settings notice and onboarding.
- Se revisaron los enlaces locales de Markdown/imágenes y la alineación de las tablas de traducción ES/EN. / Local Markdown/image links and ES/EN translation table alignment were checked.

## Firmware

Entorno / Environment: Arduino-ESP32 3.3.10, LVGL 8.3.6, NimBLE-Arduino 2.5.0. La configuración exacta está en [BUILD.es.md](BUILD.es.md) / [BUILD.md](BUILD.md).

Resultado de compilación / Build result: **correcto / passed** (Arduino CLI, salida / exit code 0).

| Medida / Metric | Bytes |
| --- | ---: |
| Programa, según Arduino CLI / Program, reported by Arduino CLI | 1,266,718 |
| RAM global / Global RAM | 107,120 |
| RAM restante, según Arduino CLI / Remaining RAM, reported by Arduino CLI | 220,560 |
| Binario de aplicación / Application binary | 1,266,976 |
| Partición `app0` / `app0` partition | 6,553,600 |

El binario cabe en la partición de aplicación incluida. Arduino CLI informa un máximo de 16 MB por la opción `Custom` de la placa; aquí se comprobó además el límite real de `app0` (6.25 MiB). Los archivos de dependencias generados confirman que el sketch y LVGL usaron el `lv_conf.h` del repositorio.

The binary fits the included application partition. Arduino CLI reports a 16 MB maximum from the board's `Custom` option; the actual `app0` limit (6.25 MiB) was also checked. Generated dependency files confirm that the sketch and LVGL used this repository's `lv_conf.h`.

SHA-256 del binario local / Local binary SHA-256: `e790568b7ffd7b6423a57439bae09879d456ee3ed6748c6ece498708000de56f`. El binario y los registros de compilación quedan en `.work/`, fuera de Git. / Build outputs and logs remain in `.work/`, outside Git.

## Alcance / Scope

El aviso se obtiene de una fuente compartida en español e inglés y se muestra completo en el asistente y Ajustes. El registro local usa la versión del aviso. No se ha probado una unidad física: quedan por comprobar presentación y desplazamiento del aviso, interacción táctil, USB/BLE, microSD, TOTP/RTC, Snatch Lock y consumo.

The notice comes from a shared Spanish/English source and is displayed in full in onboarding and Settings. The local record uses the notice version. No physical unit was tested: notice layout and scrolling, touch interaction, USB/BLE, microSD, TOTP/RTC, Snatch Lock and power consumption still require hardware verification.

No se han cargado binarios en dispositivos ni modificado eFuses. La documentación original contiene estados de aprovisionamiento de distintas revisiones; esos estados no se validan en esta preparación del repositorio.

No firmware was flashed and no eFuses were modified. Original documentation describes provisioning states from different revisions; those states are not validated by this repository preparation.
