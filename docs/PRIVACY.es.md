[English](PRIVACY.md) | **Español**

# Configurar el aviso de tu implementación

Cada persona u organización que adapte, implemente o distribuya Huskey Pocket se hace cargo de redactar, publicar, mantener y entregar el aviso de privacidad de su implementación, así como sus términos de uso cuando correspondan. Los autores del repositorio no asumen esa gestión por terceros.

## Archivo que debes editar

Edita [privacy_notice.h](../firmware/password_manager/privacy_notice.h):

| Valor | Uso |
| --- | --- |
| `HP_PRIVACY_TEXT_ES` / `HP_PRIVACY_TEXT_EN` | Aviso completo en cada idioma |
| `HP_PRIVACY_ACK_ES` / `HP_PRIVACY_ACK_EN` | Texto de la casilla del asistente |
| `HP_PRIVACY_FOOTER_ES` / `HP_PRIVACY_FOOTER_EN` | Nota al final del aviso en Ajustes |
| `HP_PRIVACY_NOTICE_VERSION` | Identificador de la versión del aviso |

El mismo aviso completo se muestra en el asistente inicial y en **Ajustes → Aviso de privacidad**. No necesitas duplicarlo en `i18n.h` ni en las pantallas. Escribe cadenas C++ con `\n\n` entre párrafos y `\"` para comillas internas. Si agregas caracteres no cubiertos por las fuentes existentes, comprueba su representación en pantalla.

## Contenido que debes definir

Identifica al responsable de tu implementación, su contacto y los canales donde entregará el aviso completo. Describe el tratamiento de datos real de tu versión, incluyendo tus cambios, las transferencias a equipos USB/BLE, QR, respaldos y exportaciones, y cualquier servicio adicional que integres. Ajusta el contenido a tu contexto de uso y a los requisitos que te correspondan.

El texto incluido es un mensaje de **aviso pendiente de configurar**, no una política lista para publicar ni un modelo de consentimiento. No atribuyas al autor del repositorio las obligaciones de tu distribución, tus garantías comerciales ni el tratamiento de datos de tus servicios.

## Lectura y versión

La casilla predeterminada dice «He leído este aviso». El asistente guarda el indicador `legal_ok` y la versión del aviso en `legal_ver`, dentro de `vprefs` en NVS; esos nombres se conservan por compatibilidad. Ese registro describe una lectura local, no demuestra consentimiento ni sustituye tu aviso.

Cambiar `HP_PRIVACY_NOTICE_VERSION` **no vuelve a abrir el asistente en equipos ya configurados**. Si tu implementación requiere presentar un aviso actualizado o registrar otras acciones, debes implementar ese flujo. No restablezcas un equipo con datos reales para probarlo: el restablecimiento elimina la bóveda.

Antes de distribuir, revisa ambos idiomas y comprueba en una unidad de prueba que el texto completo se puede desplazar en el asistente y en Ajustes. La licencia del código sigue siendo [MIT](../LICENSE); no funciona como aviso de privacidad.
