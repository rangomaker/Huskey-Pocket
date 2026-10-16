#pragma once
// =============================================================
//  sd_manager.h — MicroSD via SD_MMC para KeyVault
//  Pines Waveshare ESP32-S3-Touch-LCD-2.8: CLK=14, CMD=17, D0=16, D3=21 (modo 1-bit)
// =============================================================

#include <SD_MMC.h>
#include <FS.h>

#define SD_CLK  14
#define SD_CMD  17
#define SD_D0   16
#define SD_D1   -1 // modo 1-bit
#define SD_D2   -1 // modo 1-bit
#define SD_D3   21

static bool _sd_ready = false;

// ── Inicializar SD ────────────────────────────────────────────
bool sd_begin() {
    if (_sd_ready) return true;
    // D3 en HIGH durante power-up para modo nativo SD
    pinMode(SD_D3, OUTPUT);
    digitalWrite(SD_D3, HIGH);
    delay(10);
    if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3)) {
        Serial.println("[SD] Error: setPins falló");
        return false;
    }
    // Modo 1-bit (D0/D3 cableados)
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("[SD] Error: begin falló — sin tarjeta?");
        return false;
    }
    _sd_ready = true;
    uint64_t size = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("[SD] OK — %.0llu MB\n", size);
    return true;
}

bool sd_is_ready() { return _sd_ready; }

// Desmonta la SD antes de Deep Sleep
void sd_end() {
    if (!_sd_ready) return;
    SD_MMC.end();
    _sd_ready = false;
}

// ── Leer archivo completo como String ────────────────────────
String sd_read_file(const char* path) {
    if (!_sd_ready) return "";
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) {
        Serial.printf("[SD] No encontrado: %s\n", path);
        return "";
    }
    String content = "";
    while (f.available()) content += (char)f.read();
    f.close();
    return content;
}

// ── Verificar si existe un archivo ───────────────────────────
bool sd_exists(const char* path) {
    if (!_sd_ready) return false;
    return SD_MMC.exists(path);
}

// ── Escribir archivo ──────────────────────────────────────────
bool sd_write_file(const char* path, const String& content) {
    if (!_sd_ready) return false;
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) return false;
    f.print(content);
    f.close();
    return true;
}

// ── Escritura en chunks (para upload streaming) ───────────────
static File _sd_chunk_file;
static bool _sd_chunk_open = false;

bool sd_chunk_begin(const char* path) {
    if (!_sd_ready) return false;
    if (_sd_chunk_open) { _sd_chunk_file.close(); _sd_chunk_open = false; }
    // Crear directorio /tmp si no existe
    if (!SD_MMC.exists("/tmp")) SD_MMC.mkdir("/tmp");
    _sd_chunk_file = SD_MMC.open(path, FILE_WRITE);
    if (!_sd_chunk_file) return false;
    _sd_chunk_open = true;
    return true;
}

bool sd_chunk_write(const uint8_t* buf, size_t len) {
    if (!_sd_chunk_open || !_sd_chunk_file) return false;
    size_t written = _sd_chunk_file.write(buf, len);
    return written == len;
}

bool sd_chunk_end() {
    if (!_sd_chunk_open) return false;
    _sd_chunk_file.close();
    _sd_chunk_open = false;
    return true;
}

void sd_chunk_abort(const char* path) {
    if (_sd_chunk_open) { _sd_chunk_file.close(); _sd_chunk_open = false; }
    if (_sd_ready && SD_MMC.exists(path)) SD_MMC.remove(path);
}

// ── Eliminar archivo ─────────────────────────────────────────
bool sd_delete(const char* path) {
    if (!_sd_ready) return false;
    return SD_MMC.remove(path);
}

// ── Leer línea por línea (para importación) ──────────────────
static File _sd_line_file;
static bool _sd_line_open = false;

bool sd_readline_begin(const char* path) {
    if (!_sd_ready) return false;
    _sd_line_file = SD_MMC.open(path, FILE_READ);
    if (!_sd_line_file) return false;
    _sd_line_open = true;
    return true;
}

bool sd_readline_next(char* buf, int buf_len) {
    if (!_sd_line_open || !_sd_line_file.available()) return false;
    int i = 0;
    while (_sd_line_file.available()) {
        char c = (char)_sd_line_file.read();
        if (c == '\n') break;
        if (i < buf_len - 1) {
            if (c != '\r') buf[i++] = c;
        }
        // Si i llegó al tope, se sigue leyendo y descartando hasta el '\n':
        // así una línea sobredimensionada no se parte en varias filas basura.
    }
    buf[i] = '\0';
    return i > 0;
}

void sd_readline_end() {
    if (_sd_line_open) { _sd_line_file.close(); _sd_line_open = false; }
}

// ── Detectar tipo MIME por extensión ─────────────────────────
const char* sd_mime(const char* path) {
    String p = String(path);
    if (p.endsWith(".html")) return "text/html; charset=utf-8";
    if (p.endsWith(".css"))  return "text/css";
    if (p.endsWith(".js"))   return "application/javascript";
    if (p.endsWith(".json")) return "application/json";
    if (p.endsWith(".png"))  return "image/png";
    if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return "image/jpeg";
    if (p.endsWith(".ico"))  return "image/x-icon";
    if (p.endsWith(".woff2")) return "font/woff2";
    if (p.endsWith(".woff"))  return "font/woff";
    return "application/octet-stream";
}

#define SD_EXPORT_DIR "/export"
#define SD_IMPORT_DIR "/import"

// Crea /export y /import si no existen — llamar antes de escribir
// o listar en cualquiera de las dos.
void sd_ensure_io_dirs() {
    if (!_sd_ready) return;
    if (!SD_MMC.exists(SD_EXPORT_DIR)) SD_MMC.mkdir(SD_EXPORT_DIR);
    if (!SD_MMC.exists(SD_IMPORT_DIR)) SD_MMC.mkdir(SD_IMPORT_DIR);
}

// ── Borrado seguro: sobrescritura con datos aleatorios antes de eliminar ──
bool sd_secure_delete(const char* path) {
    if (!_sd_ready) return false;
    if (!SD_MMC.exists(path)) return true;

    // Abrir en "r+" para sobrescribir los sectores sin truncar el archivo a 0 bytes
    File f = SD_MMC.open(path, "r+");
    if (!f) return false;
    size_t size = f.size();

    uint8_t buf[512];
    size_t remaining = size;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);
        for (size_t i = 0; i < chunk; i++) buf[i] = (uint8_t)esp_random();
        f.write(buf, chunk);
        remaining -= chunk;
    }
    f.flush();
    f.close();
    return SD_MMC.remove(path);
}

// ── Limpieza de temporales al arranque ─────────────────────────
void sd_cleanup_tmp() {
    if (!_sd_ready) return;
    sd_secure_delete("/tmp/import.csv");
    sd_secure_delete("/tmp/export.csv");
    sd_secure_delete("/tmp/hky_plain.csv");
    Serial.println("[SD] Temporales limpiados (borrado seguro)");
}

// Borrado seguro de TODO el contenido de /export (no la carpeta en
// sí). Se usa al arrancar normal (por si quedó algo de una sesión
// de modo USB interrumpida) y desde el botón "Borrar export(s)" —
// nunca toca /import, que es responsabilidad exclusiva del usuario.
void sd_secure_delete_export_dir() {
    if (!_sd_ready || !SD_MMC.exists(SD_EXPORT_DIR)) return;
    File dir = SD_MMC.open(SD_EXPORT_DIR);
    if (!dir || !dir.isDirectory()) return;

    char paths[20][VAULT_FIELD_LEN];
    int count = 0;
    File f = dir.openNextFile();
    while (f && count < 20) {
        if (!f.isDirectory()) {
            String name = String(f.name());
            if (!name.startsWith(SD_EXPORT_DIR)) {
                if (name.startsWith("/")) name = name.substring(1);
                name = String(SD_EXPORT_DIR) + "/" + name;
            }
            strncpy(paths[count], name.c_str(), VAULT_FIELD_LEN - 1);
            paths[count][VAULT_FIELD_LEN - 1] = '\0';
            count++;
        }
        f = dir.openNextFile();
    }
    dir.close();
    for (int i = 0; i < count; i++) sd_secure_delete(paths[i]);
}


// ── Listar archivos .csv y .hky en /import (selector de import) ──
// Llena out[] con rutas completas ("/import/archivo.csv"), retorna
// cuántos encontró (hasta max_count). Ignora subcarpetas y ocultos.
int sd_list_import_files_root(char out[][VAULT_FIELD_LEN], int max_count) {
    if (!_sd_ready) return 0;
    sd_ensure_io_dirs();
    File root = SD_MMC.open(SD_IMPORT_DIR);
    if (!root || !root.isDirectory()) return 0;

    int count = 0;
    File f = root.openNextFile();
    while (f && count < max_count) {
        if (!f.isDirectory()) {
            String name = String(f.name());
            // Normalizar: algunos cores devuelven solo el nombre del
            // archivo, otros la ruta completa — asegurar que siempre
            // quede como "/import/archivo.ext".
            if (!name.startsWith(SD_IMPORT_DIR)) {
                if (name.startsWith("/")) name = name.substring(1);
                name = String(SD_IMPORT_DIR) + "/" + name;
            }
            String lower = name; lower.toLowerCase();
            if (lower.endsWith(".csv") || lower.endsWith(".hky")) {
                strncpy(out[count], name.c_str(), VAULT_FIELD_LEN - 1);
                out[count][VAULT_FIELD_LEN - 1] = '\0';
                count++;
            }
        }
        f = root.openNextFile();
    }
    return count;
}
