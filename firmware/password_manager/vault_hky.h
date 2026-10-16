#pragma once
// =============================================================
//  vault_hky.h — Backup cifrado (.hky) para K-Hub Pocket
//
//  Formato del archivo:
//    [4]  magic "HKY1"
//    [16] salt (PBKDF2)
//    [4]  iteraciones (uint32 LE)
//    [16] IV (AES-256-CBC)
//    [N]  ciphertext = AES256CBC( TAG(16) + csv_plano, PKCS7 )
//
//  TAG es una constante fija de 16 bytes que se cifra como primer
//  bloque — al importar, si descifra correctamente a TAG, la
//  contraseña es correcta (y el resto es el CSV real). Si no
//  coincide, contraseña incorrecta o archivo corrupto.
//
//  La llave AES se deriva con PBKDF2-HMAC-SHA256 (100,000
//  iteraciones) a partir de una contraseña que el usuario define
//  al exportar — independiente del PIN del dispositivo.
//
//  Nunca queda un CSV plano persistente en la SD ni en flash, ni en
//  exportación ni en importación: ambas cifran/descifran línea por
//  línea, al vuelo, directo contra la SD o contra el parser de
//  vault_csv.h — el CSV completo del vault jamás existe en texto
//  plano en ningún almacenamiento persistente.
//
//  Requiere include order: después de vault.h, sd_manager.h y
//  vault_csv.h (usa vault_import_csv_begin para importación; la
//  exportación ya no depende de vault_export_csv, es autónoma).
// =============================================================
#include "mbedtls/pkcs5.h"
#include "vault.h"
#include "sd_manager.h"
#include "vault_csv.h"

#define HKY_MAGIC      "HKY1"
#define HKY_ITERATIONS 100000UL
#define HKY_CHECK_TAG  "HKYPOCKETCHECK!!" // exactamente 16 bytes
#define HKY_TMP_PATH   "/tmp/hky_plain.csv" // ya no se escribe — se mantiene solo
                                             // porque ui_settings.h y sd_manager.h
                                             // aún la limpian como red de seguridad
#define HKY_HEADER_LEN (4 + 16 + 4 + 16) // magic+salt+iter+iv, sin contar el bloque TAG cifrado

static void _hky_derive_key(const char* password, const uint8_t* salt, uint32_t iterations, uint8_t* key_out) {
    mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
        (const uint8_t*)password, strlen(password),
        salt, 16, iterations, 32, key_out);
}

// ── Cifrado CBC en streaming ───────────────────────────────────
// Acumula bytes de texto plano hasta completar bloques de 16, cifra
// cada bloque completo inmediatamente y lo escribe a disco. Nunca
// existe el CSV completo en texto plano en ningún almacenamiento —
// solo fragmentos de hasta 16 bytes viven brevemente en RAM.
struct _HkyStreamEnc {
    mbedtls_aes_context aes;
    uint8_t iv_work[16];
    uint8_t leftover[16];
    size_t  leftover_len;
    File*   dst;
};

static void _hky_stream_enc_init(_HkyStreamEnc& s, const uint8_t* key, const uint8_t* iv, File* dst) {
    mbedtls_aes_init(&s.aes);
    mbedtls_aes_setkey_enc(&s.aes, key, 256);
    memcpy(s.iv_work, iv, 16);
    s.leftover_len = 0;
    s.dst = dst;
}

static void _hky_stream_enc_feed(_HkyStreamEnc& s, const uint8_t* data, size_t len) {
    size_t pos = 0;
    while (pos < len) {
        size_t need = 16 - s.leftover_len;
        size_t take = (len - pos) < need ? (len - pos) : need;
        memcpy(s.leftover + s.leftover_len, data + pos, take);
        s.leftover_len += take;
        pos += take;
        if (s.leftover_len == 16) {
            uint8_t cipher[16];
            mbedtls_aes_crypt_cbc(&s.aes, MBEDTLS_AES_ENCRYPT, 16, s.iv_work, s.leftover, cipher);
            s.dst->write(cipher, 16);
            s.leftover_len = 0;
        }
    }
}

// Aplica padding PKCS7 al remanente (0-15 bytes) y cifra el bloque final.
static void _hky_stream_enc_finish(_HkyStreamEnc& s) {
    uint8_t pad = (uint8_t)(16 - s.leftover_len);
    for (size_t i = s.leftover_len; i < 16; i++) s.leftover[i] = pad;
    uint8_t cipher[16];
    mbedtls_aes_crypt_cbc(&s.aes, MBEDTLS_AES_ENCRYPT, 16, s.iv_work, s.leftover, cipher);
    s.dst->write(cipher, 16);
    mbedtls_aes_free(&s.aes);
    memset(s.leftover, 0, 16);
}

// ── Export ──────────────────────────────────────────────────────
// Recorre el vault igual que vault_export_csv() (mismo formato de
// columnas), pero cifra cada línea al vuelo directo hacia `path` en
// SD. Nunca se escribe un CSV plano intermedio, ni en SD ni en flash.
// Retorna # de entradas exportadas, o -1 en error.
int vault_export_hky(const char* path, const char* password) {
    if (!_vault_ready) return -1;
    vault_op_busy_begin();

    char (*names)[VAULT_FIELD_LEN] = (char(*)[VAULT_FIELD_LEN])
        ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES_FS);
    if (!names) { vault_op_busy_end(); return -1; }
    int count = 0;
    vault_list(names, &count, VAULT_MAX_ENTRIES_FS, CAT_ALL);
    if (count == 0) { free(names); vault_op_busy_end(); return 0; }

    File dst = SD_MMC.open(path, FILE_WRITE);
    if (!dst) { free(names); vault_op_busy_end(); return -1; }

    uint8_t salt[16], iv[16];
    for (int i = 0; i < 4; i++) { uint32_t r = esp_random(); memcpy(salt + i*4, &r, 4); }
    for (int i = 0; i < 4; i++) { uint32_t r = esp_random(); memcpy(iv   + i*4, &r, 4); }
    uint32_t iterations = HKY_ITERATIONS;

    uint8_t key[32];
    _hky_derive_key(password, salt, iterations, key);

    dst.write((const uint8_t*)HKY_MAGIC, 4);
    dst.write(salt, 16);
    dst.write((const uint8_t*)&iterations, 4);
    dst.write(iv, 16);

    _HkyStreamEnc enc;
    _hky_stream_enc_init(enc, key, iv, &dst);

    // Primer bloque: el TAG de verificación (exactamente 16 bytes —
    // completa el buffer y se cifra/escribe de inmediato).
    _hky_stream_enc_feed(enc, (const uint8_t*)HKY_CHECK_TAG, 16);

    const char* header = "nombre,tipo,usuario,contrasena,secreto,categoria,notas\n";
    _hky_stream_enc_feed(enc, (const uint8_t*)header, strlen(header));

    int exported = 0;
    for (int i = 0; i < count; i++) {
        VaultEntry e = vault_load(names[i]);
        if (!e.valid) continue;

        String cat_name = "";
        if (strlen(e.category_id) > 0 && strcmp(e.category_id, "none") != 0) {
            VaultCategory cats[VAULT_MAX_CATS + 6];
            int cc = 0;
            vault_cat_list(cats, &cc, VAULT_MAX_CATS + 6);
            for (int j = 0; j < cc; j++) {
                if (strcmp(cats[j].id, e.category_id) == 0) {
                    cat_name = String(cats[j].name);
                    break;
                }
            }
        }

        auto escape = [](const char* val) -> String {
            String s = String(val);
            if (s.indexOf(',') >= 0 || s.indexOf('"') >= 0 || s.indexOf('\n') >= 0) {
                s.replace("\"", "\"\"");
                return "\"" + s + "\"";
            }
            return s;
        };

        String notes_flat = String(e.notes);
        notes_flat.replace("\r", " "); notes_flat.replace("\n", " ");

        String line = "";
        if (e.type == VAULT_TYPE_WEB) {
            line = escape(e.name) + ",web," +
                   escape(e.username) + "," +
                   escape(e.password) + ",," +
                   escape(cat_name.c_str()) + "," +
                   escape(notes_flat.c_str()) + "\n";
        } else {
            line = escape(e.name) + ",secret,,," +
                   escape(e.secret) + "," +
                   escape(cat_name.c_str()) + "," +
                   escape(notes_flat.c_str()) + "\n";
        }

        _hky_stream_enc_feed(enc, (const uint8_t*)line.c_str(), line.length());
        exported++;
    }

    _hky_stream_enc_finish(enc);
    dst.flush();
    dst.close();
    free(names);
    memset(key, 0, 32);

    vault_op_busy_end();
    return exported;
}

// =============================================================
//  .hky: Exportación incremental (begin/step/progress/result)
//  El contexto de cifrado en streaming vive entre ticks — unas pocas
//  entradas por llamada desde un lv_timer, con progreso real en UI.
//  Nota: begin() es bloqueante ~1s por el PBKDF2 (100k iteraciones);
//  la UI debe pintar la pantalla de progreso ANTES de llamarlo.
// =============================================================
struct HkyExportState {
    bool active   = false;
    bool failed   = false;
    int  idx      = 0;
    int  count    = 0;
    int  exported = 0;
    char (*names)[VAULT_FIELD_LEN] = nullptr;
    File dst;
    _HkyStreamEnc enc;
    char path[96] = {0};
};
static HkyExportState _hexp;

static void _hky_export_cleanup(bool abort_file) {
    if (_hexp.dst) _hexp.dst.close();
    if (abort_file && _hexp.path[0]) SD_MMC.remove(_hexp.path);
    if (_hexp.names) { free(_hexp.names); _hexp.names = nullptr; }
    _hexp.active = false;
    vault_op_busy_end();
}

bool vault_export_hky_begin(const char* path, const char* password) {
    if (!_vault_ready || _hexp.active) return false;
    vault_op_busy_begin();
    _hexp = HkyExportState();
    _hexp.active = true;
    strncpy(_hexp.path, path, sizeof(_hexp.path) - 1);

    _hexp.names = (char(*)[VAULT_FIELD_LEN])
        ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES_FS);
    if (!_hexp.names) { _hexp.failed = true; _hky_export_cleanup(false); return false; }
    vault_list(_hexp.names, &_hexp.count, VAULT_MAX_ENTRIES_FS, CAT_ALL);

    _hexp.dst = SD_MMC.open(path, FILE_WRITE);
    if (!_hexp.dst) { _hexp.failed = true; _hky_export_cleanup(false); return false; }

    uint8_t salt[16], iv[16];
    for (int i = 0; i < 4; i++) { uint32_t r = esp_random(); memcpy(salt + i*4, &r, 4); }
    for (int i = 0; i < 4; i++) { uint32_t r = esp_random(); memcpy(iv   + i*4, &r, 4); }
    uint32_t iterations = HKY_ITERATIONS;

    uint8_t key[32];
    _hky_derive_key(password, salt, iterations, key); // ~1s (PBKDF2 100k)

    _hexp.dst.write((const uint8_t*)HKY_MAGIC, 4);
    _hexp.dst.write(salt, 16);
    _hexp.dst.write((const uint8_t*)&iterations, 4);
    _hexp.dst.write(iv, 16);

    _hky_stream_enc_init(_hexp.enc, key, iv, &_hexp.dst);
    memset(key, 0, 32);

    _hky_stream_enc_feed(_hexp.enc, (const uint8_t*)HKY_CHECK_TAG, 16);
    _hky_stream_enc_feed(_hexp.enc, (const uint8_t*)CSV_EXPORT_HEADER, strlen(CSV_EXPORT_HEADER));
    return true;
}

// true = queda trabajo; false = terminó (ver vault_export_hky_result)
bool vault_export_hky_step(int batch) {
    if (!_hexp.active) return false;
    for (int b = 0; b < batch && _hexp.idx < _hexp.count; b++, _hexp.idx++) {
        VaultEntry e = vault_load(_hexp.names[_hexp.idx]);
        if (!e.valid) continue;
        String line = _csv_export_line(e); // mismo formato que vault_csv.h
        _hky_stream_enc_feed(_hexp.enc, (const uint8_t*)line.c_str(), line.length());
        _hexp.exported++;
    }
    if (_hexp.idx >= _hexp.count) {
        _hky_stream_enc_finish(_hexp.enc);
        _hexp.dst.flush();
        _hky_export_cleanup(false);
        return false;
    }
    return true;
}

void vault_export_hky_progress(int* done, int* total, bool* active) {
    if (done)   *done   = _hexp.idx;
    if (total)  *total  = _hexp.count;
    if (active) *active = _hexp.active;
}

int vault_export_hky_result() {
    return _hexp.failed ? -1 : _hexp.exported;
}

// ── Descifrado en streaming para IMPORTACIÓN ────────────────────
// Alimenta el parser incremental de vault_csv.h línea por línea,
// descifrando sobre la marcha. El CSV completo del vault importado
// nunca se materializa en texto plano en SD ni en flash — solo
// fragmentos de hasta 512 bytes viven brevemente en RAM.
struct _HkyDecCtx {
    File    src;
    mbedtls_aes_context aes;
    uint8_t iv_work[16];
    size_t  remaining;   // bytes de ciphertext restantes por leer
    uint8_t out_buf[512];
    size_t  out_len;
    size_t  out_pos;
    bool    open;
};
static _HkyDecCtx _hky_dec;

// Abre el archivo, deriva la clave, verifica el TAG y deja el cursor
// listo para leer el ciphertext del CSV. 0=ok, -1=corrupto,
// -2=contraseña incorrecta, -3=error de E/S.
static int _hky_dec_open(const char* path, const char* password) {
    _hky_dec.src = SD_MMC.open(path, FILE_READ);
    if (!_hky_dec.src) return -3;

    size_t total = _hky_dec.src.size();
    if (total < HKY_HEADER_LEN + 16) { _hky_dec.src.close(); return -1; }

    uint8_t magic[4];
    if (_hky_dec.src.read(magic, 4) != 4 || memcmp(magic, HKY_MAGIC, 4) != 0) { _hky_dec.src.close(); return -1; }
    uint8_t salt[16];
    if (_hky_dec.src.read(salt, 16) != 16) { _hky_dec.src.close(); return -1; }
    uint32_t iterations = 0;
    if (_hky_dec.src.read((uint8_t*)&iterations, 4) != 4) { _hky_dec.src.close(); return -1; }
    // No confiar en el conteo del header: un archivo malicioso con iterations
    // bajo aceleraría un ataque offline sobre la contraseña de exportación.
    if (iterations < HKY_ITERATIONS) { _hky_dec.src.close(); return -1; }
    uint8_t iv[16];
    if (_hky_dec.src.read(iv, 16) != 16) { _hky_dec.src.close(); return -1; }

    uint8_t key[32];
    _hky_derive_key(password, salt, iterations, key);
    mbedtls_aes_init(&_hky_dec.aes);
    mbedtls_aes_setkey_dec(&_hky_dec.aes, key, 256);
    memcpy(_hky_dec.iv_work, iv, 16);
    memset(key, 0, 32);

    uint8_t tag_cipher[16], tag_plain[16];
    if (_hky_dec.src.read(tag_cipher, 16) != 16) { mbedtls_aes_free(&_hky_dec.aes); _hky_dec.src.close(); return -3; }
    mbedtls_aes_crypt_cbc(&_hky_dec.aes, MBEDTLS_AES_DECRYPT, 16, _hky_dec.iv_work, tag_cipher, tag_plain);
    if (memcmp(tag_plain, HKY_CHECK_TAG, 16) != 0) {
        mbedtls_aes_free(&_hky_dec.aes);
        _hky_dec.src.close();
        return -2;
    }

    _hky_dec.remaining = total - (HKY_HEADER_LEN + 16);
    _hky_dec.out_len = 0;
    _hky_dec.out_pos = 0;
    _hky_dec.open = true;
    return 0;
}

static bool _hky_dec_refill() {
    if (!_hky_dec.open || _hky_dec.remaining == 0) return false;
    size_t to_read = _hky_dec.remaining < 512 ? _hky_dec.remaining : 512;
    uint8_t cbuf[512];
    size_t got = _hky_dec.src.read(cbuf, to_read);
    if (got != to_read) { _hky_dec.remaining = 0; return false; } // error de E/S — cortar stream

    mbedtls_aes_crypt_cbc(&_hky_dec.aes, MBEDTLS_AES_DECRYPT, got, _hky_dec.iv_work, cbuf, _hky_dec.out_buf);
    size_t write_len = got;
    _hky_dec.remaining -= got;
    if (_hky_dec.remaining == 0) {
        // Último bloque — quitar padding PKCS7
        uint8_t pad = _hky_dec.out_buf[got - 1];
        write_len = (pad == 0 || pad > 16 || pad > got) ? 0 : got - pad;
    }
    _hky_dec.out_len = write_len;
    _hky_dec.out_pos = 0;
    return true;
}

static bool _hky_dec_read_byte(uint8_t* out) {
    if (_hky_dec.out_pos >= _hky_dec.out_len) {
        if (!_hky_dec_refill() || _hky_dec.out_len == 0) return false;
    }
    *out = _hky_dec.out_buf[_hky_dec.out_pos++];
    return true;
}

// Misma semántica de retorno que sd_readline_next().
static bool _hky_dec_readline_next(char* buf, int buf_len) {
    int i = 0;
    uint8_t c;
    while (_hky_dec_read_byte(&c)) {
        if (c == '\n') break;
        if (c != '\r' && i < buf_len - 1) buf[i++] = (char)c;
    }
    buf[i] = '\0';
    return i > 0;
}

static void _hky_dec_close() {
    if (_hky_dec.open) {
        mbedtls_aes_free(&_hky_dec.aes);
        _hky_dec.src.close();
        _hky_dec.open = false;
    }
}

// ── Import ──────────────────────────────────────────────────────
// Códigos de retorno: 0=ok (listo para vault_import_csv_step),
// -1=archivo no reconocido/corrupto, -2=contraseña incorrecta, -3=error de E/S.
int vault_import_hky_begin(const char* hky_path, const char* password) {
    vault_op_busy_begin();
    // 1. Validar contraseña y contar líneas para cálculo de progreso
    int rc = _hky_dec_open(hky_path, password);
    if (rc != 0) { vault_op_busy_end(); return rc; }

    int total = 0;
    static char count_buf[1024];
    while (_hky_dec_readline_next(count_buf, sizeof(count_buf))) total++;
    _hky_dec_close();

    // 2. Reabrir stream para alimentar el parser incremental
    rc = _hky_dec_open(hky_path, password);
    if (rc != 0) { vault_op_busy_end(); return rc; }

    int data_lines = total > 0 ? total - 1 : 0;
    if (!vault_import_csv_begin_streaming(_hky_dec_readline_next, _hky_dec_close, data_lines)) {
        _hky_dec_close();
        vault_op_busy_end();
        return -3;
    }
    return 0;
}

// Limpieza post-importación y liberación de bandera ocupada
void vault_import_hky_cleanup(const char* original_imported_path) {
    if (original_imported_path) sd_secure_delete(original_imported_path);
    vault_op_busy_end();
}
