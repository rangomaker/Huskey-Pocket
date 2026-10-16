# Cambios / Changelog

## 1.0.1 · 2026-10-16

### Español

- **Teclado deslizante:** los cambios de panel se ejecutan al soltar la tecla; se cancelan las animaciones anteriores para evitar el regreso involuntario a símbolos.
- **T9:** tecla **OK** visible que confirma el texto y conserva el último carácter pendiente, en lugar de insertar un salto de línea.
- **MicroSD:** montaje y limpieza de arranque en segundo plano, después de mostrar la pantalla de acceso. La tarjeta queda disponible al terminar la inicialización; se conserva el contenido de `/import`.
- **Diagnóstico de SD:** tiempos separados de montaje y limpieza por el puerto serie. Los errores al sobrescribir un archivo impiden su eliminación.
- **Licencias:** textos de licencia de Inter y Material Symbols incluidos con las fuentes convertidas.

### English

- **Sliding keyboard:** panel changes run on release; previous animations are cancelled to prevent an unintended return to symbols.
- **T9:** a visible **OK** key confirms the text and retains the pending character instead of inserting a newline.
- **MicroSD:** startup mounting and cleanup run in the background after the access screen is shown. The card becomes available when initialization completes; `/import` contents are preserved.
- **SD diagnostics:** separate mount and cleanup timings over serial. Failed file overwrites prevent removal.
- **Licenses:** Inter and Material Symbols license texts included with the converted fonts.
