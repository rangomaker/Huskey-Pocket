#pragma once
// =============================================================
//  vault.h — Almacenamiento cifrado AES-256-CBC + NVS
//  ESP32-S3 Password Manager — v4.0
//
//  Dos tipos de entrada:
//    VAULT_TYPE_WEB    → nombre, username, password (USB HID)
//    VAULT_TYPE_SECRET → nombre, secret (PIN/código/contraseña)
//
//  Namespaces NVS:
//    "vault"  → entradas cifradas
//    "vcat"   → categorías del usuario
// =============================================================

#include <Preferences.h>
#include <LittleFS.h>
#include "mbedtls/aes.h"
#include "mbedtls/sha256.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/md.h"
#include <string.h>
#include <stdint.h>
#include <esp_efuse.h>
#include <esp_mac.h>

// Hash FNV-1a de 32 bits — distribución uniforme, sin colisiones por prefijo
static uint32_t _fnv1a_hash(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)(*str++);
        hash *= 16777619u;
    }
    return hash;
}

// Codifica un hash de 32 bits a base36 (0-9a-z) de longitud fija
static void _hash_to_base36(uint32_t h, char* out, int len) {
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    for (int i = len - 1; i >= 0; i--) {
        out[i] = digits[h % 36];
        h /= 36;
    }
    out[len] = '\0';
}

// ── Constantes ────────────────────────────────────────────────
#define VAULT_MAX_ENTRIES     3500
#define VAULT_MAX_CATS        16
#define VAULT_FIELD_LEN       64
#define VAULT_NOTES_LEN       128
#define VAULT_CAT_LEN         32

#define PIN_DIGITS            6 // Longitud estándar de PIN (6 dígitos)
#define VAULT_ENCRYPTED_LEN   96          // 16 (IV) + ((64/16)+1)*16 padded
#define VAULT_NOTES_ENC_LEN   160         // 16 (IV) + ((128/16)+1)*16 padded
#define VAULT_ENCRYPTED_LEN_NAME VAULT_ENCRYPTED_LEN
#define VAULT_NVS_NAMESPACE   "vault"

// ── Bandera de operación sensible en curso ────────────────────────
// Usada por Modo perdido / Snatch Lock para no interrumpir una
// escritura en curso (guardado de entrada, exportación, importación,
// creación inicial de la bóveda). Volátil: vive solo en RAM.
static volatile bool _vault_op_busy = false;
bool vault_is_op_busy() { return _vault_op_busy; }
void vault_op_busy_begin() { _vault_op_busy = true; }
void vault_op_busy_end()   { _vault_op_busy = false; }

#define VAULT_CAT_NAMESPACE   "vcat"
#define VAULT_DECOY_NAMESPACE "vdecoy"
#define VAULT_DCAT_NAMESPACE  "vdcat"
#define VAULT_INDEX_KEY       "idx"
#define VAULT_PIN_KEY         "vault_ctrl"
#define VAULT_DECOY_PIN_KEY   "decoy_ctrl"
#define VAULT_THEME_KEY       "theme"     // 0=oscuro, 1=claro
#define VAULT_LOCK_KEY        "lock_min"  // minutos bloqueo auto

// ── LittleFS: almacenamiento de entradas (escala a miles, NVS no) ──
#define ENTRIES_FS_DIR        "/entries"
#define ENTRIES_INDEX_PATH    "/entries_index.bin"
#define ENTRIES_FS_MAGIC      "VE01"
#define VAULT_MAX_ENTRIES_FS  3500   // límite del índice en LittleFS (vs 64 de NVS)

#define CAT_ALL               "all"
#define CAT_NONE              "none"

// ── Tipos de entrada ──────────────────────────────────────────
#define VAULT_TYPE_WEB        0   // cuenta web: username + password + USB
#define VAULT_TYPE_SECRET     1   // secreto flexible: secret field only

// ── Estructuras ───────────────────────────────────────────────
struct VaultEntry {
    char name[VAULT_FIELD_LEN];
    char category_id[VAULT_CAT_LEN];
    char notes[VAULT_NOTES_LEN];
    char icon[VAULT_FIELD_LEN];
    char totp_secret[VAULT_FIELD_LEN]; // Base32, vacío si no aplica
    uint8_t type;                   // VAULT_TYPE_WEB o VAULT_TYPE_SECRET

    // Tipo WEB
    char username[VAULT_FIELD_LEN];
    char password[VAULT_FIELD_LEN];

    // Tipo SECRET
    char secret[VAULT_FIELD_LEN];  // PIN, código, contraseña genérica

    // Metadatos
    uint32_t created_at;            // timestamp Unix de creación
    bool     is_favorite;           // marcado como favorito

    bool valid;
};

struct VaultCategory {
    char id[VAULT_CAT_LEN];
    char name[VAULT_CAT_LEN];
    int  default_idx; // 0 = personalizada; 1-5 = carpeta de fábrica sin renombrar
                       // (la UI traduce el nombre dinámicamente vía i18n cuando es >0)
    bool valid;
};

// ── Estado interno ────────────────────────────────────────────
static uint8_t     _vault_key[32];
static bool        _vault_ready  = false;
static bool        _vault_is_decoy = false;  // true si está en vault señuelo
static Preferences _prefs;
static Preferences _prefs_cat;

// ── Funciones privadas ────────────────────────────────────────

#define VAULT_PBKDF2_ITERATIONS 50000
#define VAULT_SALT_LEN          16
#define VAULT_SALT_PATH         "/vault_salt.bin"
#define VAULT_DECOY_SALT_PATH   "/vault_decoy_salt.bin"
#define VAULT_FRP_SALT_PATH     "/vault_frp_salt.bin"

// Carga el salt desde LittleFS; si no existe, genera uno random y lo persiste.
static void _load_or_create_salt(const char* path, uint8_t* salt_out) {
    if (LittleFS.exists(path)) {
        fs::File f = LittleFS.open(path, "r");
        if (f && f.size() == VAULT_SALT_LEN) {
            f.read(salt_out, VAULT_SALT_LEN);
            f.close();
            return;
        }
        if (f) f.close();
    }
    for (int i = 0; i < VAULT_SALT_LEN; i += 4) {
        uint32_t r = esp_random();
        memcpy(salt_out + i, &r, 4);
    }
    fs::File f = LittleFS.open(path, "w");
    if (f) {
        f.write(salt_out, VAULT_SALT_LEN);
        f.close();
    }
}

static void _derive_key_into(const char* pin, const char* salt_path, uint8_t* key_out); // fwd decl

static void _derive_key(const char* pin, bool decoy = false) {
    _derive_key_into(pin, decoy ? VAULT_DECOY_SALT_PATH : VAULT_SALT_PATH, _vault_key);
}

// Verificación de PIN mediante valor de control cifrado (_verify_control)

static bool _aes_encrypt_key(const uint8_t* key, const uint8_t* plain, size_t plain_len,
                          uint8_t* out, size_t* out_len) {
    size_t padded_len = ((plain_len / 16) + 1) * 16;
    // El bloque relleno debe caber en el buffer; si no, rechazar en vez de
    // desbordar el stack con el memcpy de abajo.
    if (padded_len > VAULT_NOTES_ENC_LEN) return false;
    uint8_t padded[VAULT_NOTES_ENC_LEN] = {0};
    memcpy(padded, plain, plain_len);
    uint8_t pad = (uint8_t)(padded_len - plain_len);
    for (size_t i = plain_len; i < padded_len; i++) padded[i] = pad;

    uint8_t iv[16];
    for (int i = 0; i < 4; i++) {
        uint32_t r = esp_random();
        memcpy(iv + i * 4, &r, 4);
    }
    memcpy(out, iv, 16);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    // Comprobar los códigos de retorno de mbedtls: si la clave o el cifrado
    // fallan, no marcar éxito ni dejar datos parciales como válidos.
    if (mbedtls_aes_setkey_enc(&aes, key, 256) != 0) { mbedtls_aes_free(&aes); return false; }
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);
    int rc = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT,
                                   padded_len, iv_copy, padded, out + 16);
    mbedtls_aes_free(&aes);
    if (rc != 0) return false;
    *out_len = 16 + padded_len;
    return true;
}

static bool _aes_decrypt_key(const uint8_t* key, const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len) {
    if (in_len < 32) return false;
    size_t cipher_len = in_len - 16;
    // El ciphertext CBC debe ser múltiplo de 16; si no, el dato está corrupto
    // o manipulado (CBC no autentica) — rechazar antes de tocar mbedtls.
    if (cipher_len == 0 || cipher_len % 16 != 0) return false;
    if (cipher_len > VAULT_NOTES_ENC_LEN) return false;
    uint8_t iv[16];
    memcpy(iv, in, 16);
    uint8_t decrypted[VAULT_NOTES_ENC_LEN] = {0};

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_dec(&aes, key, 256) != 0) { mbedtls_aes_free(&aes); return false; }
    int rc = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT,
                                   cipher_len, iv, in + 16, decrypted);
    mbedtls_aes_free(&aes);
    if (rc != 0) return false;

    uint8_t pad_val = decrypted[cipher_len - 1];
    if (pad_val == 0 || pad_val > 16 || pad_val > cipher_len) return false;
    // Validar PKCS7 completo: los pad_val bytes finales deben ser todos iguales
    // a pad_val. Evita un oráculo de padding parcial sobre CBC sin MAC.
    for (size_t i = cipher_len - pad_val; i < cipher_len; i++) {
        if (decrypted[i] != pad_val) return false;
    }
    *out_len = cipher_len - pad_val;
    memcpy(out, decrypted, *out_len);
    out[*out_len] = '\0';
    return true;
}

static bool _aes_encrypt(const uint8_t* plain, size_t plain_len,
                          uint8_t* out, size_t* out_len) {
    return _aes_encrypt_key(_vault_key, plain, plain_len, out, out_len);
}

static bool _aes_decrypt(const uint8_t* in, size_t in_len,
                          uint8_t* out, size_t* out_len) {
    return _aes_decrypt_key(_vault_key, in, in_len, out, out_len);
}

// ── Valor de control cifrado ────────────────────────────────
// Reemplaza la verificación por hash: el único modo de confirmar que un
// PIN es correcto es derivar la clave completa (PBKDF2, 50k iteraciones)
// e intentar descifrar este valor fijo con ella. No existe atajo barato:
// cada intento, online u offline, paga el costo completo de PBKDF2.
#define VAULT_CTRL_MAGIC "HUSKEYOK"
#define VAULT_CTRL_LEN   32   // 16 (IV) + 16 (bloque padded de 8 bytes)

static void _write_control(Preferences& p, const char* key_name, const uint8_t* aes_key) {
    uint8_t out[VAULT_CTRL_LEN];
    size_t out_len = 0;
    _aes_encrypt_key(aes_key, (const uint8_t*)VAULT_CTRL_MAGIC, strlen(VAULT_CTRL_MAGIC), out, &out_len);
    p.putBytes(key_name, out, out_len);
}

static bool _verify_control(Preferences& p, const char* key_name, const uint8_t* aes_key) {
    if (p.getBytesLength(key_name) != VAULT_CTRL_LEN) return false;
    uint8_t buf[VAULT_CTRL_LEN];
    p.getBytes(key_name, buf, VAULT_CTRL_LEN);
    uint8_t dec[32];
    size_t dec_len = 0;
    if (!_aes_decrypt_key(aes_key, buf, VAULT_CTRL_LEN, dec, &dec_len)) return false;
    if (dec_len != strlen(VAULT_CTRL_MAGIC)) return false;
    return memcmp(dec, VAULT_CTRL_MAGIC, dec_len) == 0;
}

// Deriva una clave PBKDF2 hacia un buffer explícito, sin tocar _vault_key
// global — usado para verificar PINs sin alterar la sesión activa.
static void _derive_key_into(const char* pin, const char* salt_path, uint8_t* key_out) {
    uint8_t salt[VAULT_SALT_LEN];
    _load_or_create_salt(salt_path, salt);
    mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, (const uint8_t*)pin, strlen(pin),
                               salt, VAULT_SALT_LEN,
                               VAULT_PBKDF2_ITERATIONS, 32, key_out);
}

static bool _enc_str(const char* plain, uint8_t* out, size_t* out_len) {
    return _aes_encrypt((const uint8_t*)plain, strlen(plain), out, out_len);
}

static bool _dec_str(const uint8_t* in, size_t in_len, char* out) {
    size_t out_len = 0;
    bool ok = _aes_decrypt(in, in_len, (uint8_t*)out, &out_len);
    if (ok) out[out_len] = '\0';
    return ok;
}

// =============================================================
//  vaultfs — Slots de tamaño fijo indexados por hash (open
//  addressing, linear probing), repartidos en varios archivos
//  ("buckets") en vez de un único archivo gigante.
//
//  Por qué buckets y no un solo archivo: LittleFS organiza cada
//  archivo como una lista encadenada de bloques (CTZ skip-list).
//  Escribir en medio de un archivo grande obliga a reescribir la
//  cadena de punteros de TODOS los bloques posteriores hasta el
//  final del archivo — con un archivo de ~1.5MB esto agota el
//  espacio libre de la partición mucho antes de llenarse de verdad
//  (confirmado en pruebas reales: "No more free space" con solo
//  ~26% de los slots usados). Con archivos chicos (~23KB cada uno)
//  ese costo de reescritura es mínimo y predecible.
// =============================================================
static bool _fs_ready = false;

#define VAULTFS_SLOT_COUNT   2048              // total de slots (sin cambios de diseño)
#define VAULTFS_BUCKET_SLOTS 32                // slots por archivo bucket
#define VAULTFS_BUCKET_COUNT (VAULTFS_SLOT_COUNT / VAULTFS_BUCKET_SLOTS) // 64 archivos
#define VAULTFS_CAT_ENC_LEN  64                // cap cifrado para category_id (32 chars)

// Layout del registro (tamaño fijo):
#define VFS_STATUS_OFF   0   // 1: 0=libre, 1=ocupado, 2=tombstone (borrado)
#define VFS_TYPE_OFF     1   // 1
#define VFS_FAV_OFF      2   // 1
#define VFS_CREATED_OFF  3   // 4
#define VFS_NAMEHASH_OFF 7   // 4  hash FNV1a del nombre en claro (solo para probing rápido)
#define VFS_NAME_LEN_OFF 11  // 2
#define VFS_NAME_OFF     13  // VAULT_ENCRYPTED_LEN_NAME (96)
#define VFS_CAT_LEN_OFF  (VFS_NAME_OFF + VAULT_ENCRYPTED_LEN_NAME)        // 109
#define VFS_CAT_OFF      (VFS_CAT_LEN_OFF + 2)                            // 111
#define VFS_USER_LEN_OFF (VFS_CAT_OFF + VAULTFS_CAT_ENC_LEN)              // 175
#define VFS_USER_OFF     (VFS_USER_LEN_OFF + 2)                          // 177
#define VFS_PASS_LEN_OFF (VFS_USER_OFF + VAULT_ENCRYPTED_LEN)             // 273
#define VFS_PASS_OFF     (VFS_PASS_LEN_OFF + 2)                          // 275
#define VFS_SECR_LEN_OFF (VFS_PASS_OFF + VAULT_ENCRYPTED_LEN)             // 371
#define VFS_SECR_OFF     (VFS_SECR_LEN_OFF + 2)                          // 373
#define VFS_NOTE_LEN_OFF (VFS_SECR_OFF + VAULT_ENCRYPTED_LEN)             // 469
#define VFS_NOTE_OFF     (VFS_NOTE_LEN_OFF + 2)                          // 471
#define VFS_ICON_LEN_OFF (VFS_NOTE_OFF + VAULT_NOTES_ENC_LEN)             // 631
#define VFS_ICON_OFF     (VFS_ICON_LEN_OFF + 2)                          // 633
#define VFS_TOTP_LEN_OFF (VFS_ICON_OFF + VAULT_ENCRYPTED_LEN_NAME)        // 729
#define VFS_TOTP_OFF     (VFS_TOTP_LEN_OFF + 2)                          // 731
#define VAULTFS_REC_SIZE (VFS_TOTP_OFF + VAULT_ENCRYPTED_LEN)             // 827

#define VFS_ST_FREE      0
#define VFS_ST_OCCUPIED  1
#define VFS_ST_TOMBSTONE 2

// Ruta del archivo bucket que contiene el slot global dado.
static void _vaultfs_bucket_path(int32_t slot, char* out, size_t out_len) {
    int bucket = slot / VAULTFS_BUCKET_SLOTS;
    snprintf(out, out_len, "/vb_%02d.bin", bucket);
}

// Offset DENTRO del bucket (no global) para el slot dado.
static inline uint32_t _vaultfs_local_offset(int32_t slot) {
    return (uint32_t)(slot % VAULTFS_BUCKET_SLOTS) * VAULTFS_REC_SIZE;
}

// Monta LittleFS en la partición "entriesfs" y crea los buckets
// (uno por uno, ~23KB cada uno) si no existen.
static bool vaultfs_init() {
    if (_fs_ready) return true;
    vault_op_busy_begin();
    if (!LittleFS.begin(false, "/littlefs", 10, "entriesfs")) {
        Serial.println("[VFS] Mount falló, formateando...");
        if (!LittleFS.begin(true, "/littlefs", 10, "entriesfs")) {
            Serial.println("[VFS] Error fatal: no se pudo montar LittleFS");
            vault_op_busy_end();
            return false;
        }
    }
    _fs_ready = true;

    char path[24];
    uint8_t zeros[VAULTFS_REC_SIZE] = {0}; // status=0=FREE para todos los campos
    for (int b = 0; b < VAULTFS_BUCKET_COUNT; b++) {
        snprintf(path, sizeof(path), "/vb_%02d.bin", b);
        if (LittleFS.exists(path)) continue;

        fs::File f = LittleFS.open(path, "w");
        if (!f) { Serial.printf("[VFS] Error: no se pudo crear %s\n", path); _fs_ready = false; vault_op_busy_end(); return false; }
        bool ok = true;
        for (int i = 0; i < VAULTFS_BUCKET_SLOTS && ok; i++) {
            ok = (f.write(zeros, VAULTFS_REC_SIZE) == VAULTFS_REC_SIZE);
            yield(); // 2048 escrituras seguidas sin ceder CPU dispara el watchdog de interrupciones con WiFi activo
        }
        f.close();
        if (!ok) { LittleFS.remove(path); Serial.printf("[VFS] Error inicializando %s\n", path); _fs_ready = false; vault_op_busy_end(); return false; }
        yield();
    }
    Serial.printf("[VFS] %d buckets listos — %d slots totales, %d bytes/bucket\n",
                  VAULTFS_BUCKET_COUNT, VAULTFS_SLOT_COUNT, VAULTFS_BUCKET_SLOTS * VAULTFS_REC_SIZE);
    vault_op_busy_end();
    return true;
}

// ── Versión por pasos de vaultfs_init — para el onboarding ──────
// Crea los buckets de pocos en pocos en vez de los 64 de una sola
// vez, así LVGL puede repintar la pantalla entre cada paso (no acorta
// el tiempo total, solo evita que se vea congelada mientras tanto).
static int  _vfs_init_step_idx = 0;

static bool vaultfs_init_begin() {
    if (_fs_ready) { _vfs_init_step_idx = VAULTFS_BUCKET_COUNT; return true; }
    vault_op_busy_begin();
    if (!LittleFS.begin(false, "/littlefs", 10, "entriesfs")) {
        Serial.println("[VFS] Mount falló, formateando...");
        if (!LittleFS.begin(true, "/littlefs", 10, "entriesfs")) {
            Serial.println("[VFS] Error fatal: no se pudo montar LittleFS");
            return false;
        }
    }
    _fs_ready = true;
    _vfs_init_step_idx = 0;
    return true;
}

// Procesa hasta 'buckets_per_step' buckets. true = quedan más, false = terminó.
static bool vaultfs_init_step(int buckets_per_step) {
    char path[24];
    uint8_t zeros[VAULTFS_REC_SIZE] = {0};
    int processed = 0;
    while (_vfs_init_step_idx < VAULTFS_BUCKET_COUNT && processed < buckets_per_step) {
        int b = _vfs_init_step_idx;
        snprintf(path, sizeof(path), "/vb_%02d.bin", b);
        if (!LittleFS.exists(path)) {
            fs::File f = LittleFS.open(path, "w");
            if (!f) {
                Serial.printf("[VFS] Error: no se pudo crear %s\n", path);
            } else {
                bool ok = true;
                for (int i = 0; i < VAULTFS_BUCKET_SLOTS && ok; i++) {
                    ok = (f.write(zeros, VAULTFS_REC_SIZE) == VAULTFS_REC_SIZE);
                    yield();
                }
                f.close();
                if (!ok) { LittleFS.remove(path); Serial.printf("[VFS] Error inicializando %s\n", path); }
            }
        }
        _vfs_init_step_idx++;
        processed++;
    }
    if (_vfs_init_step_idx >= VAULTFS_BUCKET_COUNT) {
        Serial.printf("[VFS] %d buckets listos (por pasos) — %d slots totales\n",
                      VAULTFS_BUCKET_COUNT, VAULTFS_SLOT_COUNT);
        vault_op_busy_end();
        return false;
    }
    return true;
}

static int vaultfs_init_progress_pct() {
    return (_vfs_init_step_idx * 100) / VAULTFS_BUCKET_COUNT;
}

// Borra todos los archivos bucket (usado por factory reset).
static void vaultfs_remove_all_buckets() {
    char path[24];
    for (int b = 0; b < VAULTFS_BUCKET_COUNT; b++) {
        snprintf(path, sizeof(path), "/vb_%02d.bin", b);
        if (LittleFS.exists(path)) LittleFS.remove(path);
    }
}

// Escribe un registro completo (todos los campos cifrados) en el slot dado.
static bool _vaultfs_write_record(int32_t slot, const VaultEntry& e) {
    char path[24];
    _vaultfs_bucket_path(slot, path, sizeof(path));
    fs::File f = LittleFS.open(path, "r+");
    if (!f) { Serial.printf("[VFS] FALLO: no se pudo abrir %s en r+\n", path); return false; }
    if (!f.seek(_vaultfs_local_offset(slot))) { Serial.printf("[VFS] FALLO: seek a slot %ld\n", (long)slot); f.close(); return false; }

    uint8_t rec[VAULTFS_REC_SIZE];
    memset(rec, 0, VAULTFS_REC_SIZE);
    rec[VFS_STATUS_OFF] = VFS_ST_OCCUPIED;
    rec[VFS_TYPE_OFF]   = e.type;
    rec[VFS_FAV_OFF]    = e.is_favorite ? 1 : 0;
    memcpy(rec + VFS_CREATED_OFF, &e.created_at, 4);
    uint32_t nh = _fnv1a_hash(e.name);
    memcpy(rec + VFS_NAMEHASH_OFF, &nh, 4);

    struct { int len_off, data_off, cap; const char* val; } fields[] = {
        { VFS_NAME_LEN_OFF, VFS_NAME_OFF, VAULT_ENCRYPTED_LEN_NAME, e.name },
        { VFS_CAT_LEN_OFF,  VFS_CAT_OFF,  VAULTFS_CAT_ENC_LEN,      e.category_id },
        { VFS_USER_LEN_OFF, VFS_USER_OFF, VAULT_ENCRYPTED_LEN,      e.username },
        { VFS_PASS_LEN_OFF, VFS_PASS_OFF, VAULT_ENCRYPTED_LEN,      e.password },
        { VFS_SECR_LEN_OFF, VFS_SECR_OFF, VAULT_ENCRYPTED_LEN,      e.secret },
        { VFS_NOTE_LEN_OFF, VFS_NOTE_OFF, VAULT_NOTES_ENC_LEN,      e.notes },
        { VFS_ICON_LEN_OFF, VFS_ICON_OFF, VAULT_ENCRYPTED_LEN_NAME, e.icon },
        { VFS_TOTP_LEN_OFF, VFS_TOTP_OFF, VAULT_ENCRYPTED_LEN,      e.totp_secret },
    };
    uint8_t enc[VAULT_NOTES_ENC_LEN];
    for (auto& fld : fields) {
        size_t enc_len = 0;
        if (!_enc_str(fld.val, enc, &enc_len)) { Serial.println("[VFS] FALLO: _enc_str"); f.close(); return false; }
        if (enc_len > (size_t)fld.cap) { Serial.printf("[VFS] FALLO: campo excede cap (%u > %d)\n", (unsigned)enc_len, fld.cap); f.close(); return false; }
        uint16_t len16 = (uint16_t)enc_len;
        memcpy(rec + fld.len_off, &len16, 2);
        memcpy(rec + fld.data_off, enc, enc_len);
    }

    size_t written = f.write(rec, VAULTFS_REC_SIZE);
    f.flush();
    f.close();
    if (written != VAULTFS_REC_SIZE) {
        Serial.printf("[VFS] FALLO: f.write escribió %u/%u bytes (¿sin espacio?)\n", (unsigned)written, (unsigned)VAULTFS_REC_SIZE);
        return false;
    }
    return true;
}

// Lee y desencripta el registro completo del slot dado.
static bool _vaultfs_read_record(int32_t slot, VaultEntry& e) {
    char path[24];
    _vaultfs_bucket_path(slot, path, sizeof(path));
    fs::File f = LittleFS.open(path, "r");
    if (!f) return false;
    if (!f.seek(_vaultfs_local_offset(slot))) { f.close(); return false; }

    uint8_t rec[VAULTFS_REC_SIZE];
    if (f.read(rec, VAULTFS_REC_SIZE) != VAULTFS_REC_SIZE) { f.close(); return false; }
    f.close();

    if (rec[VFS_STATUS_OFF] != VFS_ST_OCCUPIED) return false;

    e = VaultEntry{};
    e.type = rec[VFS_TYPE_OFF];
    e.is_favorite = rec[VFS_FAV_OFF] != 0;
    memcpy(&e.created_at, rec + VFS_CREATED_OFF, 4);

    struct { int len_off, data_off; char* out; size_t out_cap; } fields[] = {
        { VFS_NAME_LEN_OFF, VFS_NAME_OFF, e.name,        VAULT_FIELD_LEN },
        { VFS_CAT_LEN_OFF,  VFS_CAT_OFF,  e.category_id, VAULT_CAT_LEN },
        { VFS_USER_LEN_OFF, VFS_USER_OFF, e.username,    VAULT_FIELD_LEN },
        { VFS_PASS_LEN_OFF, VFS_PASS_OFF, e.password,    VAULT_FIELD_LEN },
        { VFS_SECR_LEN_OFF, VFS_SECR_OFF, e.secret,      VAULT_FIELD_LEN },
        { VFS_NOTE_LEN_OFF, VFS_NOTE_OFF, e.notes,       VAULT_NOTES_LEN },
        { VFS_ICON_LEN_OFF, VFS_ICON_OFF, e.icon,        VAULT_FIELD_LEN },
        { VFS_TOTP_LEN_OFF, VFS_TOTP_OFF, e.totp_secret, VAULT_FIELD_LEN },
    };
    for (auto& fld : fields) {
        uint16_t len16 = 0;
        memcpy(&len16, rec + fld.len_off, 2);
        if (len16 == 0) { fld.out[0] = '\0'; continue; }
        if (!_dec_str(rec + fld.data_off, len16, fld.out)) return false;
    }
    e.valid = true;
    return true;
}

// Busca el slot de `name` (open addressing, linear probing, atravesando
// buckets de forma transparente). Si existe: *out_found=true, retorna su
// slot. Si no: *out_found=false, retorna el primer slot libre/tombstone
// encontrado en la cadena (para insertar ahí), o -1 si está lleno.
static int32_t _vaultfs_locate(const char* name, bool* out_found) {
    *out_found = false;
    if (!vaultfs_init()) return -1;

    uint32_t target_hash = _fnv1a_hash(name);
    uint32_t start = target_hash & (VAULTFS_SLOT_COUNT - 1);
    int32_t insert_slot = -1;

    fs::File f;            // handle del bucket actualmente abierto
    int open_bucket = -1;  // índice del bucket con el handle abierto (-1 = ninguno)
    char path[24];

    for (int32_t i = 0; i < VAULTFS_SLOT_COUNT; i++) {
        int32_t slot = (int32_t)((start + i) % VAULTFS_SLOT_COUNT);
        int bucket = slot / VAULTFS_BUCKET_SLOTS;

        if (bucket != open_bucket) {
            if (f) f.close();
            _vaultfs_bucket_path(slot, path, sizeof(path));
            f = LittleFS.open(path, "r");
            if (!f) break;
            open_bucket = bucket;
        }

        if (!f.seek(_vaultfs_local_offset(slot))) break;
        uint8_t header[VFS_NAME_LEN_OFF]; // status+type+fav+created+namehash
        if (f.read(header, sizeof(header)) != sizeof(header)) break;

        uint8_t status = header[VFS_STATUS_OFF];
        if (status == VFS_ST_FREE) {
            if (insert_slot < 0) insert_slot = slot;
            break; // fin de la cadena de probing: el nombre no existe
        }
        if (status == VFS_ST_TOMBSTONE) {
            if (insert_slot < 0) insert_slot = slot;
            continue; // sigue probando, puede haber colisión más adelante
        }
        // OCCUPIED: comparar hash primero (barato), confirmar con nombre real
        uint32_t stored_hash;
        memcpy(&stored_hash, header + VFS_NAMEHASH_OFF, 4);
        if (stored_hash == target_hash) {
            uint16_t nlen = 0;
            f.seek(_vaultfs_local_offset(slot) + VFS_NAME_LEN_OFF);
            f.read((uint8_t*)&nlen, 2);
            uint8_t enc[VAULT_ENCRYPTED_LEN_NAME];
            char decoded[VAULT_FIELD_LEN];
            if (nlen > 0 && nlen <= VAULT_ENCRYPTED_LEN_NAME) {
                f.seek(_vaultfs_local_offset(slot) + VFS_NAME_OFF);
                f.read(enc, nlen);
                if (_dec_str(enc, nlen, decoded) && strcmp(decoded, name) == 0) {
                    f.close();
                    *out_found = true;
                    return slot;
                }
            }
        }
    }
    if (f) f.close();
    return insert_slot;
}

// Marca el slot de `name` como tombstone (borrado), sobrescribiendo todo
// el registro con ceros primero. No usa FREE — FREE cortaría la cadena
// de probing y rompería búsquedas de otras entradas colisionadas en hash.
static bool _vaultfs_delete_by_name(const char* name) {
    bool found = false;
    int32_t slot = _vaultfs_locate(name, &found);
    if (!found) return false;

    // Reutilizar el mismo path; abrir UNA sola vez en r+ (lectura+escritura)
    // en vez del patrón anterior "r" en locate + reapertura "r+" aquí.
    // (Nota: el abort en __sfp que motivó este cambio resultó ser una fuga
    // de pantallas LVGL en _load_screen_instant, no este patrón; se conserva
    // la apertura única por ser más eficiente.)
    char path[24];
    _vaultfs_bucket_path(slot, path, sizeof(path));
    fs::File f = LittleFS.open(path, "r+");
    if (!f) return false;
    if (!f.seek(_vaultfs_local_offset(slot))) { f.close(); return false; }

    uint8_t rec[VAULTFS_REC_SIZE];
    memset(rec, 0, VAULTFS_REC_SIZE);
    rec[VFS_STATUS_OFF] = VFS_ST_TOMBSTONE;
    bool ok = (f.write(rec, VAULTFS_REC_SIZE) == VAULTFS_REC_SIZE);
    f.flush();
    f.close();
    return ok;
}


// =============================================================
//  Fase 2 — Índice en RAM (PSRAM) sobre vaultfs.
//  Se construye una vez por sesión (al desbloquear), con una sola
//  pasada secuencial de /vault.bin. Mientras la sesión está activa,
//  vault_list/vault_list_meta/vault_count/etc. leen de aquí, no de
//  disco. Se actualiza incrementalmente en cada save/delete — nunca
//  se reescanea el archivo completo salvo al desbloquear.
//  Se libera por completo al bloquear (vaultfs_index_clear).
// =============================================================
struct VaultRamEntry {
    char     name[VAULT_FIELD_LEN];
    char     username[VAULT_FIELD_LEN];     // solo tiene sentido para tipo WEB
    char     category_id[VAULT_CAT_LEN];
    uint8_t  type;
    bool     favorite;
    uint32_t created_at;
    int32_t  slot;                          // slot físico en /vault.bin
};

static VaultRamEntry* _ram_index       = nullptr;  // PSRAM, capacidad VAULTFS_SLOT_COUNT
static int            _ram_index_count = 0;
static bool           _ram_index_ready = false;

// Decodifica un registro crudo (leído del archivo) hacia VaultRamEntry,
// sin necesidad de pasar por VaultEntry completo (evita desencriptar
// password/secret/notes/icon, que NO se cachean en RAM).
static bool _ram_decode_record(const uint8_t* rec, int32_t slot, VaultRamEntry& out) {
    if (rec[VFS_STATUS_OFF] != VFS_ST_OCCUPIED) return false;
    out.slot     = slot;
    out.type     = rec[VFS_TYPE_OFF];
    out.favorite = rec[VFS_FAV_OFF] != 0;
    memcpy(&out.created_at, rec + VFS_CREATED_OFF, 4);

    uint16_t nlen = 0, ulen = 0, clen = 0;
    memcpy(&nlen, rec + VFS_NAME_LEN_OFF, 2);
    memcpy(&ulen, rec + VFS_USER_LEN_OFF, 2);
    memcpy(&clen, rec + VFS_CAT_LEN_OFF, 2);

    if (nlen == 0 || nlen > VAULT_ENCRYPTED_LEN_NAME) return false;
    if (!_dec_str(rec + VFS_NAME_OFF, nlen, out.name)) return false;

    if (ulen == 0) out.username[0] = '\0';
    else if (ulen <= VAULT_ENCRYPTED_LEN) { if (!_dec_str(rec + VFS_USER_OFF, ulen, out.username)) out.username[0]='\0'; }
    else out.username[0] = '\0';

    if (clen == 0) strncpy(out.category_id, "none", VAULT_CAT_LEN-1);
    else if (clen <= VAULTFS_CAT_ENC_LEN) { if (!_dec_str(rec + VFS_CAT_OFF, clen, out.category_id)) strncpy(out.category_id, "none", VAULT_CAT_LEN-1); }
    else strncpy(out.category_id, "none", VAULT_CAT_LEN-1);

    return true;
}

// Construye el índice completo — una sola pasada secuencial por todos
// los buckets. Llamar UNA vez al desbloquear (tras tener _vault_key listo).
static bool vaultfs_index_build() {
    if (!vaultfs_init()) return false;

    if (!_ram_index) {
        _ram_index = (VaultRamEntry*)ps_malloc(sizeof(VaultRamEntry) * VAULTFS_SLOT_COUNT);
        if (!_ram_index) { Serial.println("[VFS] FATAL: sin PSRAM para índice RAM"); return false; }
    }
    _ram_index_count = 0;

    uint8_t rec[VAULTFS_REC_SIZE];
    char path[24];
    for (int b = 0; b < VAULTFS_BUCKET_COUNT; b++) {
        snprintf(path, sizeof(path), "/vb_%02d.bin", b);
        fs::File f = LittleFS.open(path, "r");
        if (!f) continue;

        int32_t slot = b * VAULTFS_BUCKET_SLOTS;
        while (f.read(rec, VAULTFS_REC_SIZE) == VAULTFS_REC_SIZE) {
            if (rec[VFS_STATUS_OFF] == VFS_ST_OCCUPIED) {
                VaultRamEntry e;
                if (_ram_decode_record(rec, slot, e)) {
                    _ram_index[_ram_index_count++] = e;
                }
            }
            slot++;
        }
        f.close();
        yield(); // evitar watchdog de interrupciones con WiFi activo, igual que en vaultfs_init
    }

    _ram_index_ready = true;
    Serial.printf("[VFS] Índice RAM construido: %d entradas\n", _ram_index_count);
    return true;
}

// Libera el índice de RAM (llamar al bloquear/auto-bloquear el vault).
static void vaultfs_index_clear() {
    if (_ram_index) {
        memset(_ram_index, 0, sizeof(VaultRamEntry) * VAULTFS_SLOT_COUNT);
        free(_ram_index);
        _ram_index = nullptr;
    }
    _ram_index_count = 0;
    _ram_index_ready = false;
}

// Busca por nombre dentro del índice en RAM. Retorna índice o -1.
static int _ram_index_find(const char* name) {
    if (!_ram_index_ready) return -1;
    for (int i = 0; i < _ram_index_count; i++)
        if (strcmp(_ram_index[i].name, name) == 0) return i;
    return -1;
}

// Busca por slot físico (usado tras guardar, para no tener que comparar
// strings cuando ya sabemos el slot exacto).
static int _ram_index_find_by_slot(int32_t slot) {
    if (!_ram_index_ready) return -1;
    for (int i = 0; i < _ram_index_count; i++)
        if (_ram_index[i].slot == slot) return i;
    return -1;
}

// Inserta o actualiza una entrada en el índice de RAM (llamar después de
// cada vaultfs_save exitoso — mantiene el índice sincronizado sin reescanear).
static void vaultfs_index_upsert(int32_t slot, const VaultEntry& e) {
    if (!_ram_index_ready || !_ram_index) return;

    VaultRamEntry re;
    re.slot = slot;
    re.type = e.type;
    re.favorite = e.is_favorite;
    re.created_at = e.created_at;
    strncpy(re.name, e.name, VAULT_FIELD_LEN - 1); re.name[VAULT_FIELD_LEN-1] = '\0';
    strncpy(re.username, e.type == VAULT_TYPE_WEB ? e.username : "", VAULT_FIELD_LEN - 1); re.username[VAULT_FIELD_LEN-1] = '\0';
    strncpy(re.category_id, strlen(e.category_id) ? e.category_id : "none", VAULT_CAT_LEN - 1); re.category_id[VAULT_CAT_LEN-1] = '\0';

    int idx = _ram_index_find_by_slot(slot);
    if (idx < 0) idx = _ram_index_find(e.name); // por si el slot cambió (no debería, pero por seguridad)

    if (idx >= 0) _ram_index[idx] = re;
    else if (_ram_index_count < VAULTFS_SLOT_COUNT) _ram_index[_ram_index_count++] = re;
}

// Quita una entrada del índice de RAM (llamar después de borrar en disco).
static void vaultfs_index_remove(const char* name) {
    if (!_ram_index_ready) return;
    int idx = _ram_index_find(name);
    if (idx < 0) return;
    // Mover la última al hueco — el orden no importa, es solo una tabla.
    _ram_index[idx] = _ram_index[_ram_index_count - 1];
    _ram_index_count--;
}

static bool _fs_begin() {
    if (_fs_ready) return true;
    // false = no formatear si falla el mount (evita borrar datos por error transitorio)
    if (!LittleFS.begin(false, "/littlefs", 10, "entriesfs")) {
        Serial.println("[FS] Mount falló, formateando...");
        if (!LittleFS.begin(true, "/littlefs", 10, "entriesfs")) {
            Serial.println("[FS] Error fatal: no se pudo montar LittleFS");
            return false;
        }
    }
    if (!LittleFS.exists(ENTRIES_FS_DIR)) LittleFS.mkdir(ENTRIES_FS_DIR);
    _fs_ready = true;
    Serial.println("[FS] LittleFS listo");
    return true;
}

// Path del archivo de una entrada a partir de su nombre (hash, evita
// problemas con caracteres especiales/longitud en nombres de archivo)
static void _entry_file_path(const char* name, char* out, size_t out_len) {
    char hash_str[7];
    _hash_to_base36(_fnv1a_hash(name), hash_str, 6);
    snprintf(out, out_len, "%s/%s.bin", ENTRIES_FS_DIR, hash_str);
}

// Escribe un campo cifrado: [2 bytes len][bytes cifrados]
static bool _fs_write_enc_field(fs::File& f, const char* plain) {
    uint8_t enc[VAULT_NOTES_ENC_LEN];
    size_t enc_len = 0;
    if (!_aes_encrypt((const uint8_t*)plain, strlen(plain), enc, &enc_len)) return false;
    uint16_t len16 = (uint16_t)enc_len;
    if (f.write((uint8_t*)&len16, 2) != 2) return false;
    if (f.write(enc, enc_len) != enc_len) return false;
    return true;
}

// Lee un campo cifrado: [2 bytes len][bytes cifrados] -> string plano en out
static bool _fs_read_enc_field(fs::File& f, char* out, size_t out_cap) {
    uint16_t len16 = 0;
    if (f.read((uint8_t*)&len16, 2) != 2) return false;
    if (len16 == 0 || len16 > VAULT_NOTES_ENC_LEN) return false;
    uint8_t enc[VAULT_NOTES_ENC_LEN];
    if (f.read(enc, len16) != len16) return false;
    return _dec_str(enc, len16, out);
}

// Guarda una entrada completa en su archivo individual.
// Sobrescribe si ya existe (mismo hash de nombre).
struct IndexEntryMeta {
    char name[VAULT_FIELD_LEN];
    char username[VAULT_FIELD_LEN];
    uint8_t type;
    bool favorite;
};
static bool entries_fs_index_upsert(const IndexEntryMeta& meta);

static bool entries_fs_save(const VaultEntry& e) {
    if (!_fs_begin()) return false;
    char path[48];
    _entry_file_path(e.name, path, sizeof(path));

    fs::File f = LittleFS.open(path, "w");
    if (!f) return false;

    bool ok = true;
    ok &= f.write((const uint8_t*)ENTRIES_FS_MAGIC, 4) == 4;
    ok &= f.write(&e.type, 1) == 1;
    uint8_t fav = e.is_favorite ? 1 : 0;
    ok &= f.write(&fav, 1) == 1;
    ok &= f.write((const uint8_t*)&e.created_at, 4) == 4;
    ok &= _fs_write_enc_field(f, e.category_id);
    ok &= _fs_write_enc_field(f, e.name);
    ok &= _fs_write_enc_field(f, e.username);
    ok &= _fs_write_enc_field(f, e.password);
    ok &= _fs_write_enc_field(f, e.secret);
    ok &= _fs_write_enc_field(f, e.notes);
    ok &= _fs_write_enc_field(f, e.icon);
    f.close();

    if (!ok) { LittleFS.remove(path); return false; }

    // Mantener el índice sincronizado (name+username+type+favorite)
    // para que listar la pantalla principal no requiera abrir este archivo.
    IndexEntryMeta meta = {};
    strncpy(meta.name, e.name, VAULT_FIELD_LEN - 1);
    if (e.type == VAULT_TYPE_WEB) strncpy(meta.username, e.username, VAULT_FIELD_LEN - 1);
    meta.type = e.type;
    meta.favorite = e.is_favorite;
    entries_fs_index_upsert(meta);

    return true;
}

// Carga una entrada por nombre. e.valid queda en false si no existe o falla.
static VaultEntry entries_fs_load(const char* name) {
    VaultEntry e = {};
    e.valid = false;
    if (!_fs_begin()) return e;

    char path[48];
    _entry_file_path(name, path, sizeof(path));
    if (!LittleFS.exists(path)) return e;

    fs::File f = LittleFS.open(path, "r");
    if (!f) return e;

    char magic[5] = {0};
    bool ok = true;
    ok &= f.read((uint8_t*)magic, 4) == 4;
    ok &= (strncmp(magic, ENTRIES_FS_MAGIC, 4) == 0);
    ok &= f.read(&e.type, 1) == 1;
    uint8_t fav = 0;
    ok &= f.read(&fav, 1) == 1;
    e.is_favorite = (fav != 0);
    ok &= f.read((uint8_t*)&e.created_at, 4) == 4;
    ok &= _fs_read_enc_field(f, e.category_id, VAULT_CAT_LEN);
    ok &= _fs_read_enc_field(f, e.name, VAULT_FIELD_LEN);
    ok &= _fs_read_enc_field(f, e.username, VAULT_FIELD_LEN);
    ok &= _fs_read_enc_field(f, e.password, VAULT_FIELD_LEN);
    ok &= _fs_read_enc_field(f, e.secret, VAULT_FIELD_LEN);
    ok &= _fs_read_enc_field(f, e.notes, VAULT_NOTES_LEN);
    ok &= _fs_read_enc_field(f, e.icon, VAULT_FIELD_LEN);
    f.close();

    e.valid = ok;
    return e;
}

static bool entries_fs_delete(const char* name) {
    if (!_fs_begin()) return false;
    char path[48];
    _entry_file_path(name, path, sizeof(path));
    if (!LittleFS.exists(path)) return false;
    return LittleFS.remove(path);
}

// ── Índice: registros de tamaño FIJO en disco, uno por entrada.
//    Incluye name+username+type+favorite cifrados para poder listar
//    la pantalla principal SIN abrir cada archivo individual (eso era
//    el cuello de botella: 63 entradas = 63 aperturas+descifrados AES
//    solo para mostrar la lista). Permite append y borrar in-place. ──
#define IDX_REC_SIZE      199
#define IDX_DELETED_OFF   0
#define IDX_TYPE_OFF      1
#define IDX_FAV_OFF       2
#define IDX_NAME_LEN_OFF  3
#define IDX_NAME_OFF      5
#define IDX_NAME_CAP      VAULT_ENCRYPTED_LEN_NAME            // 96B, cubre IV+padding real
#define IDX_USER_LEN_OFF  (IDX_NAME_OFF + IDX_NAME_CAP)        // 101
#define IDX_USER_OFF      (IDX_USER_LEN_OFF + 2)               // 103
#define IDX_USER_CAP      (IDX_REC_SIZE - IDX_USER_OFF)        // 96

// Recorre el índice llamando cb(meta, record_offset) por cada registro activo.
// No carga el archivo completo a RAM — lee un registro a la vez (streaming).
typedef void (*IndexVisitCb)(const IndexEntryMeta& meta, uint32_t rec_offset, void* ctx);

static bool _idx_decode_record(const uint8_t* rec, IndexEntryMeta& out) {
    out.type = rec[IDX_TYPE_OFF];
    out.favorite = rec[IDX_FAV_OFF] != 0;

    uint16_t nlen = rec[IDX_NAME_LEN_OFF] | (rec[IDX_NAME_LEN_OFF + 1] << 8);
    if (nlen == 0 || nlen > IDX_NAME_CAP) return false;
    if (!_dec_str(rec + IDX_NAME_OFF, nlen, out.name)) return false;

    uint16_t ulen = rec[IDX_USER_LEN_OFF] | (rec[IDX_USER_LEN_OFF + 1] << 8);
    if (ulen == 0) { out.username[0] = '\0'; }
    else {
        if (ulen > IDX_USER_CAP) return false;
        if (!_dec_str(rec + IDX_USER_OFF, ulen, out.username)) return false;
    }
    return true;
}

static bool _idx_encode_record(const IndexEntryMeta& meta, uint8_t* rec_out) {
    memset(rec_out, 0, IDX_REC_SIZE);
    rec_out[IDX_DELETED_OFF] = 0;
    rec_out[IDX_TYPE_OFF] = meta.type;
    rec_out[IDX_FAV_OFF] = meta.favorite ? 1 : 0;

    uint8_t enc[VAULT_ENCRYPTED_LEN];
    size_t enc_len = 0;
    if (!_aes_encrypt((const uint8_t*)meta.name, strlen(meta.name), enc, &enc_len)) return false;
    if (enc_len > IDX_NAME_CAP) return false;
    rec_out[IDX_NAME_LEN_OFF] = (uint8_t)(enc_len & 0xFF);
    rec_out[IDX_NAME_LEN_OFF + 1] = (uint8_t)((enc_len >> 8) & 0xFF);
    memcpy(rec_out + IDX_NAME_OFF, enc, enc_len);

    if (strlen(meta.username) > 0) {
        size_t ulen = 0;
        if (!_aes_encrypt((const uint8_t*)meta.username, strlen(meta.username), enc, &ulen)) return false;
        if (ulen > IDX_USER_CAP) ulen = 0; // truncar silenciosamente si excede (caso raro)
        rec_out[IDX_USER_LEN_OFF] = (uint8_t)(ulen & 0xFF);
        rec_out[IDX_USER_LEN_OFF + 1] = (uint8_t)((ulen >> 8) & 0xFF);
        if (ulen > 0) memcpy(rec_out + IDX_USER_OFF, enc, ulen);
    }
    return true;
}

static void entries_fs_index_visit(IndexVisitCb cb, void* ctx) {
    if (!_fs_begin()) return;
    if (!LittleFS.exists(ENTRIES_INDEX_PATH)) return;

    fs::File f = LittleFS.open(ENTRIES_INDEX_PATH, "r");
    if (!f) return;

    uint8_t rec[IDX_REC_SIZE];
    IndexEntryMeta meta;
    uint32_t offset = 0;

    while (f.read(rec, IDX_REC_SIZE) == IDX_REC_SIZE) {
        if (rec[IDX_DELETED_OFF] == 0) {
            if (_idx_decode_record(rec, meta)) cb(meta, offset, ctx);
        }
        offset += IDX_REC_SIZE;
    }
    f.close();
}

// Lee hasta max_items nombres activos del índice (compatibilidad con
// vault_list, que solo necesita nombres para la mayoría de usos).
struct _IdxReadCtx { char (*out)[VAULT_FIELD_LEN]; int max; int count; };
static void _idx_read_cb(const IndexEntryMeta& meta, uint32_t off, void* ctx) {
    _IdxReadCtx* c = (_IdxReadCtx*)ctx;
    if (c->count >= c->max) return;
    strncpy(c->out[c->count], meta.name, VAULT_FIELD_LEN - 1);
    c->out[c->count][VAULT_FIELD_LEN - 1] = '\0';
    c->count++;
}
static int entries_fs_index_list(char names_out[][VAULT_FIELD_LEN], int max_items) {
    _IdxReadCtx ctx = { names_out, max_items, 0 };
    entries_fs_index_visit(_idx_read_cb, &ctx);
    return ctx.count;
}

// Lee hasta max_items metadatos completos (name+username+type+favorite),
// para renderizar la lista principal sin abrir cada archivo de entrada.
struct _IdxReadMetaCtx { IndexEntryMeta* out; int max; int count; };
static void _idx_read_meta_cb(const IndexEntryMeta& meta, uint32_t off, void* ctx) {
    _IdxReadMetaCtx* c = (_IdxReadMetaCtx*)ctx;
    if (c->count >= c->max) return;
    c->out[c->count++] = meta;
}
static int entries_fs_index_list_meta(IndexEntryMeta* out, int max_items) {
    _IdxReadMetaCtx ctx = { out, max_items, 0 };
    entries_fs_index_visit(_idx_read_meta_cb, &ctx);
    return ctx.count;
}

// Busca el registro activo con ese nombre. Retorna -1 si no existe.
struct _IdxFindCtx { const char* target; int32_t found_offset; IndexEntryMeta found_meta; };
static void _idx_find_cb(const IndexEntryMeta& meta, uint32_t off, void* ctx) {
    _IdxFindCtx* c = (_IdxFindCtx*)ctx;
    if (c->found_offset < 0 && strcmp(meta.name, c->target) == 0) {
        c->found_offset = (int32_t)off;
        c->found_meta = meta;
    }
}
static int32_t _entries_fs_index_find(const char* name) {
    _IdxFindCtx ctx = { name, -1, {} };
    entries_fs_index_visit(_idx_find_cb, &ctx);
    return ctx.found_offset;
}

// Agrega o actualiza (si ya existe) el registro de meta.name —
// usado por entries_fs_save para que el índice quede sincronizado
// con username/type/favorite cada vez que se guarda una entrada.
static bool entries_fs_index_upsert(const IndexEntryMeta& meta) {
    if (!_fs_begin()) return false;

    _IdxFindCtx ctx = { meta.name, -1, {} };
    entries_fs_index_visit(_idx_find_cb, &ctx);

    uint8_t rec[IDX_REC_SIZE];
    if (!_idx_encode_record(meta, rec)) return false;

    if (ctx.found_offset >= 0) {
        // Actualizar in-place (mismo tamaño de registro siempre)
        fs::File f = LittleFS.open(ENTRIES_INDEX_PATH, "r+");
        if (!f) return false;
        if (!f.seek(ctx.found_offset)) { f.close(); return false; }
        bool ok = f.write(rec, IDX_REC_SIZE) == IDX_REC_SIZE;
        f.close();
        return ok;
    } else {
        // Append al final
        fs::File f = LittleFS.open(ENTRIES_INDEX_PATH, LittleFS.exists(ENTRIES_INDEX_PATH) ? "a" : "w");
        if (!f) return false;
        bool ok = f.write(rec, IDX_REC_SIZE) == IDX_REC_SIZE;
        f.close();
        return ok;
    }
}

// Mantiene compatibilidad con el código que solo necesita registrar el
// nombre (sin username/type/favorite todavía conocidos en ese punto).
static bool entries_fs_index_add(const char* name) {
    if (_entries_fs_index_find(name) >= 0) return true;
    IndexEntryMeta meta = {};
    strncpy(meta.name, name, VAULT_FIELD_LEN - 1);
    return entries_fs_index_upsert(meta);
}

// Marca como borrado el registro de ese nombre (in-place, sin reescribir el resto).
// Compacta automáticamente el índice si el espacio desperdiciado (registros
// borrados) supera el 20% del total, para que el archivo no crezca sin límite.
static bool entries_fs_index_compact_if_needed();

static bool entries_fs_index_remove(const char* name) {
    int32_t off = _entries_fs_index_find(name);
    if (off < 0) return true; // no estaba, nada que hacer

    fs::File f = LittleFS.open(ENTRIES_INDEX_PATH, "r+");
    if (!f) return false;
    if (!f.seek(off)) { f.close(); return false; }
    uint8_t deleted_flag = 1;
    bool ok = f.write(&deleted_flag, 1) == 1;
    f.close();
    if (ok) entries_fs_index_compact_if_needed();
    return ok;
}

// Reescribe el índice quedándose solo con los registros activos,
// eliminando los "huecos" dejados por entries_fs_index_remove.
static bool entries_fs_index_compact() {
    if (!_fs_begin()) return false;
    if (!LittleFS.exists(ENTRIES_INDEX_PATH)) return true;

    const char* tmp_path = "/entries_index.tmp";
    fs::File src = LittleFS.open(ENTRIES_INDEX_PATH, "r");
    if (!src) return false;
    fs::File dst = LittleFS.open(tmp_path, "w");
    if (!dst) { src.close(); return false; }

    uint8_t rec[IDX_REC_SIZE];
    bool ok = true;
    while (src.read(rec, IDX_REC_SIZE) == IDX_REC_SIZE) {
        if (rec[IDX_DELETED_OFF] == 0) {
            if (dst.write(rec, IDX_REC_SIZE) != IDX_REC_SIZE) { ok = false; break; }
        }
    }
    src.close();
    dst.close();

    if (!ok) { LittleFS.remove(tmp_path); return false; }

    LittleFS.remove(ENTRIES_INDEX_PATH);
    return LittleFS.rename(tmp_path, ENTRIES_INDEX_PATH);
}

// Compacta solo si el desperdicio (% de registros borrados) supera el 20%.
// Evita compactar en cada borrado individual (sería tan costoso como el
// problema que queremos resolver) — solo cuando realmente vale la pena.
static bool entries_fs_index_compact_if_needed() {
    if (!_fs_begin()) return false;
    if (!LittleFS.exists(ENTRIES_INDEX_PATH)) return true;

    fs::File f = LittleFS.open(ENTRIES_INDEX_PATH, "r");
    if (!f) return false;

    uint32_t total = 0, deleted = 0;
    uint8_t rec[IDX_REC_SIZE];
    while (f.read(rec, IDX_REC_SIZE) == IDX_REC_SIZE) {
        total++;
        if (rec[IDX_DELETED_OFF] != 0) deleted++;
    }
    f.close();

    if (total == 0) return true;
    float waste_ratio = (float)deleted / (float)total;
    if (waste_ratio < 0.20f) return true; // no vale la pena compactar todavía

    Serial.printf("[FS] Compactando índice: %u/%u registros borrados (%.0f%%)\n",
                   deleted, total, waste_ratio * 100);
    return entries_fs_index_compact();
}

// =============================================================
//  Fase 3 — Migración: backend viejo (LittleFS un-archivo-por-
//  entrada) -> nuevo /vault.bin (slots fijos indexados por hash).
//  Corre una sola vez (controlado por flag en NVS). Las entradas
//  viejas NO se borran hasta confirmar que todas migraron bien —
//  si algo falla a mitad de camino, el backend viejo sigue intacto
//  y se puede reintentar en el siguiente arranque.
// =============================================================
#define VFS_MIGRATION_FLAG_KEY  "vfs_mig_ok"

static bool vaultfs_migration_done() {
    Preferences p;
    p.begin(VAULT_NVS_NAMESPACE, true);
    bool done = p.getBool(VFS_MIGRATION_FLAG_KEY, false);
    p.end();
    return done;
}

static void _vaultfs_mark_migration_done() {
    Preferences p;
    p.begin(VAULT_NVS_NAMESPACE, false);
    p.putBool(VFS_MIGRATION_FLAG_KEY, true);
    p.end();
}

// Migra entradas del formato individual al formato binario por buckets (vaultfs).
static bool vaultfs_migrate_if_needed() {
    if (vaultfs_migration_done()) return true;
    if (!vaultfs_init())   return false;
    if (!_fs_begin())      return false; // backend viejo también debe poder montar

    Serial.println("[VFS] Iniciando migración a /vault.bin...");

    char (*old_names)[VAULT_FIELD_LEN] = (char(*)[VAULT_FIELD_LEN])
        ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES_FS);
    if (!old_names) { Serial.println("[VFS] FALLO: sin PSRAM para migración"); return false; }

    int old_count = entries_fs_index_list(old_names, VAULT_MAX_ENTRIES_FS);
    Serial.printf("[VFS] %d entradas encontradas en el backend viejo\n", old_count);

    int migrated = 0, failed = 0;
    for (int i = 0; i < old_count; i++) {
        VaultEntry e = entries_fs_load(old_names[i]);
        if (!e.valid) { failed++; Serial.printf("[VFS] No se pudo leer (viejo): %s\n", old_names[i]); continue; }

        bool found = false;
        int32_t slot = _vaultfs_locate(e.name, &found);
        if (slot < 0) { failed++; Serial.printf("[VFS] Sin slot libre para: %s\n", e.name); continue; }

        if (!_vaultfs_write_record(slot, e)) { failed++; Serial.printf("[VFS] FALLO al escribir: %s\n", e.name); continue; }
        migrated++;
    }
    free(old_names);

    Serial.printf("[VFS] Migración: %d OK, %d fallidas de %d\n", migrated, failed, old_count);

    if (failed > 0) {
        Serial.println("[VFS] Migración incompleta — se reintentará en el próximo arranque. Backend viejo intacto.");
        return false;
    }

    _vaultfs_mark_migration_done();
    Serial.println("[VFS] Migración completa y marcada como hecha.");
    return true;
}

// Elimina archivos de almacenamiento anteriores una vez verificada la migración
static bool vaultfs_purge_old_backend() {
    if (!vaultfs_migration_done()) {
        Serial.println("[VFS] No se purga: la migración no está confirmada.");
        return false;
    }
    if (!_fs_begin()) return false;

    char (*old_names)[VAULT_FIELD_LEN] = (char(*)[VAULT_FIELD_LEN])
        ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES_FS);
    if (!old_names) return false;

    int old_count = entries_fs_index_list(old_names, VAULT_MAX_ENTRIES_FS);
    for (int i = 0; i < old_count; i++) entries_fs_delete(old_names[i]);
    free(old_names);

    if (LittleFS.exists(ENTRIES_INDEX_PATH)) LittleFS.remove(ENTRIES_INDEX_PATH);
    Serial.printf("[VFS] Backend viejo purgado: %d archivos eliminados\n", old_count);
    return true;
}

// Clave NVS: máx 15 chars — "xxxxxx_ff"
// Usa un hash del nombre completo (no un prefijo literal) para evitar que
// nombres distintos que comparten los primeros caracteres (ej. "Fearlessrevolution"
// y "Fearlessrevolution 2") colisionen y se sobrescriban entre sí.
static void _nvs_key(const char* name, const char* field, char* out) {
    char hash_str[7];
    _hash_to_base36(_fnv1a_hash(name), hash_str, 6);
    snprintf(out, 15, "%s_%s", hash_str, field);
}

static void _init_default_categories() {
    _prefs_cat.begin(VAULT_CAT_NAMESPACE, false);
    bool already = _prefs_cat.getBool("defaults_ok", false);
    if (!already) {
        // Limpiar cualquier dato previo parcial
        _prefs_cat.clear();
        // Marcadores independientes de idioma — la UI los traduce dinámicamente
        // vía i18n (t(K_CAT_*)) cada vez que se muestran, así que respetan el
        // idioma activo en todo momento, no solo el que había al crearse.
        // Si el usuario renombra una, deja de ser marcador y se vuelve texto
        // fijo normal (igual que cualquier carpeta personalizada).
        // "Sin categoria" ya no se guarda aquí — es redundante con el sentinel
        // "none" que ya maneja category_id vacío (ver t(K_SIN_CATEGORIA)).
        const char* markers[] = {
            "DEFAULT_1","DEFAULT_2","DEFAULT_3","DEFAULT_4","DEFAULT_5"
        };
        String idx = "";
        for (int i = 0; i < 5; i++) {
            String nk = "c" + String(i) + "_name";
            _prefs_cat.putString(nk.c_str(), markers[i]);
            idx += String(i) + ",";
        }
        _prefs_cat.putInt("cat_count", 5);
        _prefs_cat.putString("cat_idx", idx);
        _prefs_cat.putBool("defaults_ok", true);
        Serial.println("[VAULT] Categorias por defecto creadas");
    }
    _prefs_cat.end();
}

// ── API: Autenticación ────────────────────────────────────────

// ── Factory Reset Protection (FRP) ───────────────────────────
#define VAULT_FRP_NAMESPACE "vfrp"

void vault_frp_save_pin(const char* pin) {
    uint8_t frp_key[32];
    _derive_key_into(pin, VAULT_FRP_SALT_PATH, frp_key);
    Preferences p;
    p.begin(VAULT_FRP_NAMESPACE, false);
    _write_control(p, "frp_ctrl", frp_key);
    p.end();
    memset(frp_key, 0, 32);
}

bool vault_frp_verify(const char* pin) {
    uint8_t frp_key[32];
    _derive_key_into(pin, VAULT_FRP_SALT_PATH, frp_key);
    Preferences p;
    p.begin(VAULT_FRP_NAMESPACE, true);
    bool ok = _verify_control(p, "frp_ctrl", frp_key);
    p.end();
    memset(frp_key, 0, 32);
    return ok;
}

bool vault_frp_is_set() {
    Preferences p;
    p.begin(VAULT_FRP_NAMESPACE, true);
    bool set = p.getBytesLength("frp_ctrl") == VAULT_CTRL_LEN;
    p.end();
    return set;
}

void vault_frp_lock_permanent() {
    Preferences p;
    p.begin(VAULT_FRP_NAMESPACE, false);
    p.putBool("frp_locked", true);
    p.end();
}

// Contador de intentos fallidos de FRP — persistente en el namespace vfrp,
// que sobrevive al factory reset. Evita el bypass por corte de energía.
int vault_frp_get_attempts() {
    Preferences p;
    p.begin(VAULT_FRP_NAMESPACE, true);
    int v = p.getInt("frp_att", 0);
    p.end();
    return v;
}

void vault_frp_set_attempts(int n) {
    Preferences p;
    p.begin(VAULT_FRP_NAMESPACE, false);
    p.putInt("frp_att", n);
    p.end();
}

void vault_frp_reset_attempts() {
    Preferences p;
    p.begin(VAULT_FRP_NAMESPACE, false);
    p.putInt("frp_att", 0);
    p.end();
}

bool vault_frp_is_locked_permanent() {
    Preferences p;
    p.begin(VAULT_FRP_NAMESPACE, true);
    bool locked = p.getBool("frp_locked", false);
    p.end();
    return locked;
}

// ── Intentos fallidos de PIN ──────────────────────────────────
#define VAULT_SEC_NAMESPACE "vsec"

int vault_get_fail_count() {
    Preferences p;
    p.begin(VAULT_SEC_NAMESPACE, true);
    int v = p.getInt("fail_count", 0);
    p.end();
    return v;
}

void vault_set_fail_count(int count) {
    Preferences p;
    p.begin(VAULT_SEC_NAMESPACE, false);
    p.putInt("fail_count", count);
    p.end();
}

void vault_reset_fail_count() {
    Preferences p;
    p.begin(VAULT_SEC_NAMESPACE, false);
    p.putInt("fail_count", 0);
    p.end();
}

// Saber si ya existe un PIN registrado, sin necesidad de desbloquear —
// para que el flujo de onboarding sepa si debe mostrarse o no.
bool vault_has_pin() {
    Preferences p;
    p.begin(VAULT_NVS_NAMESPACE, true);
    bool has = p.getBytesLength(VAULT_PIN_KEY) == VAULT_CTRL_LEN;
    p.end();
    return has;
}

static void _vault_set_last_unlock_ts(uint32_t ts); // definida más abajo (usa RTC)
static uint32_t _vault_unix_now(); // definida más abajo

bool vault_init(const char* pin) {
    // Asegurar LittleFS montado antes de leer/crear el salt de PBKDF2
    if (!LittleFS.begin(false, "/littlefs", 10, "entriesfs")) {
        LittleFS.begin(true, "/littlefs", 10, "entriesfs");
    }

    // Derivación de clave simétrica para PIN real y señuelo (mitigación de análisis de tiempo)
    uint8_t cand_decoy[32];
    uint8_t cand_real[32];
    _derive_key_into(pin, VAULT_DECOY_SALT_PATH, cand_decoy);
    _derive_key_into(pin, VAULT_SALT_PATH,       cand_real);

    // Verificación del señuelo
    Preferences _decoy_check;
    _decoy_check.begin(VAULT_NVS_NAMESPACE, true);
    bool has_decoy = _decoy_check.getBytesLength(VAULT_DECOY_PIN_KEY) == VAULT_CTRL_LEN;
    bool decoy_ok  = has_decoy && _verify_control(_decoy_check, VAULT_DECOY_PIN_KEY, cand_decoy);
    _decoy_check.end();

    if (decoy_ok) {
        _prefs.begin(VAULT_DECOY_NAMESPACE, false);
        _prefs_cat.begin(VAULT_DCAT_NAMESPACE, false);
        memcpy(_vault_key, cand_decoy, 32);
        memset(cand_decoy, 0, 32);
        memset(cand_real,  0, 32);
        _vault_ready    = true;
        _vault_is_decoy = true;
        Serial.println("[VAULT] Vault señuelo activado");
        return true;
    }

    // PIN normal
    _prefs.begin(VAULT_NVS_NAMESPACE, false);

    if (_prefs.getBytesLength(VAULT_PIN_KEY) != VAULT_CTRL_LEN) {
        // Primera vez — registrar PIN
        _write_control(_prefs, VAULT_PIN_KEY, cand_real);
        _prefs.putInt(VAULT_THEME_KEY, 0);
        _prefs.putInt(VAULT_LOCK_KEY, -1);  // default: 30 seg
        // Guardar en FRP — sobrevive al reset de fábrica
        vault_frp_save_pin(pin);
        Serial.println("[VAULT] PIN registrado — primer uso");
    } else if (!_verify_control(_prefs, VAULT_PIN_KEY, cand_real)) {
        _prefs.end();
        memset(cand_decoy, 0, 32);
        memset(cand_real,  0, 32);
        return false;
    }
    memcpy(_vault_key, cand_real, 32);
    memset(cand_decoy, 0, 32);
    memset(cand_real,  0, 32);
    _vault_ready    = true;
    _vault_is_decoy = false;
    _vault_set_last_unlock_ts(_vault_unix_now()); // mano muerta: solo cuenta desbloqueos reales
    _init_default_categories();

    // Backend nuevo: migrar datos viejos si hace falta, y construir el
    // índice en RAM para esta sesión. El señuelo NO usa almacenamiento
    // real, así que esto solo corre para el vault verdadero.
    vaultfs_migrate_if_needed();
    vaultfs_index_build();

    return true;
}

void vault_lock() {
    memset(_vault_key, 0, 32);
    _vault_ready    = false;
    _vault_is_decoy = false;
    vaultfs_index_clear();
    _prefs.end();
}

bool vault_is_decoy() { return _vault_is_decoy; }

// Confirma un PIN (real o señuelo) sin alterar la sesión activa — usado
// para pedir confirmación antes de acciones sensibles.
bool vault_verify_pin(const char* pin) {
    uint8_t candidate[32];
    bool ok = false;

    if (_vault_ready) {
        // Vault ya abierto — reusar _prefs (NVS no permite dos handles
        // concurrentes al mismo namespace)
        if (_prefs.getBytesLength(VAULT_PIN_KEY) == VAULT_CTRL_LEN) {
            _derive_key_into(pin, VAULT_SALT_PATH, candidate);
            ok = _verify_control(_prefs, VAULT_PIN_KEY, candidate);
        }
        if (!ok && _prefs.getBytesLength(VAULT_DECOY_PIN_KEY) == VAULT_CTRL_LEN) {
            _derive_key_into(pin, VAULT_DECOY_SALT_PATH, candidate);
            ok = _verify_control(_prefs, VAULT_DECOY_PIN_KEY, candidate);
        }
    } else {
        Preferences p;
        p.begin(VAULT_NVS_NAMESPACE, true);
        if (p.getBytesLength(VAULT_PIN_KEY) == VAULT_CTRL_LEN) {
            _derive_key_into(pin, VAULT_SALT_PATH, candidate);
            ok = _verify_control(p, VAULT_PIN_KEY, candidate);
        }
        if (!ok && p.getBytesLength(VAULT_DECOY_PIN_KEY) == VAULT_CTRL_LEN) {
            _derive_key_into(pin, VAULT_DECOY_SALT_PATH, candidate);
            ok = _verify_control(p, VAULT_DECOY_PIN_KEY, candidate);
        }
        p.end();
    }
    memset(candidate, 0, 32);
    return ok;
}


bool vault_is_ready()              { return _vault_ready; }

// ── API: PIN señuelo ──────────────────────────────────────────
bool vault_decoy_is_set() {
    Preferences p;
    p.begin(VAULT_NVS_NAMESPACE, true);
    bool set = p.getBytesLength(VAULT_DECOY_PIN_KEY) == VAULT_CTRL_LEN;
    p.end();
    return set;
}

void vault_decoy_set_pin(const char* pin) {
    uint8_t decoy_key[32];
    _derive_key_into(pin, VAULT_DECOY_SALT_PATH, decoy_key);
    Preferences p;
    p.begin(VAULT_NVS_NAMESPACE, false);
    _write_control(p, VAULT_DECOY_PIN_KEY, decoy_key);
    p.end();
    memset(decoy_key, 0, 32);
}

void vault_decoy_clear_pin() {
    Preferences p;
    p.begin(VAULT_NVS_NAMESPACE, false);
    p.remove(VAULT_DECOY_PIN_KEY);
    p.end();
    // Limpiar vault señuelo también
    Preferences pd;
    pd.begin(VAULT_DECOY_NAMESPACE, false);
    pd.clear();
    pd.end();
    Preferences pdc;
    pdc.begin(VAULT_DCAT_NAMESPACE, false);
    pdc.clear();
    pdc.end();
}

// Tiempo de auto reset del PIN señuelo (en minutos, default 5)
int vault_decoy_get_reset_time() {
    Preferences p;
    p.begin(VAULT_NVS_NAMESPACE, true);
    int t = p.getInt("decoy_reset_min", 5);
    p.end();
    return t;
}

void vault_decoy_set_reset_time(int minutes) {
    Preferences p;
    p.begin(VAULT_NVS_NAMESPACE, false);
    p.putInt("decoy_reset_min", minutes);
    p.end();
}

// ── API: Tema sin cifrar — accesible antes del unlock ─────────
// Guardado en namespace "vprefs" independiente del vault

int vault_get_theme_raw() {
    Preferences p;
    p.begin("vprefs", true);
    int t = p.getInt("theme", 0);
    p.end();
    return t;
}

void vault_set_theme_raw(int t) {
    Preferences p;
    p.begin("vprefs", false);
    p.putInt("theme", t);
    p.end();
}

int vault_get_accent_raw() {
    Preferences p;
    p.begin("vprefs", true);
    int a = p.getInt("accent", 0);
    p.end();
    return a;
}

void vault_set_accent_raw(int a) {
    Preferences p;
    p.begin("vprefs", false);
    p.putInt("accent", a);
    p.end();
}

// Tema Personalizado — fondo/tarjeta/acento elegidos con Hue+Intensidad.
// Se guarda como bytes crudos en "vprefs"/"custom_thm"; si la key no existe
// (primera vez), Preferences::getBytes no toca el struct y quedan los
// defaults de abajo.
struct VaultCustomTheme {
    int16_t base;   // 0=oscuro, 1=claro (base de brillo para fondo/tarjeta)
    int16_t bg_h, bg_s;
    int16_t card_h, card_s;
    int16_t acc_h, acc_s;
};

VaultCustomTheme vault_get_custom_theme() {
    VaultCustomTheme c = { 0, 210, 8, 210, 14, 211, 96 }; // defaults
    Preferences p;
    p.begin("vprefs", true);
    p.getBytes("custom_thm", &c, sizeof(c));
    p.end();
    return c;
}

void vault_set_custom_theme(const VaultCustomTheme& c) {
    Preferences p;
    p.begin("vprefs", false);
    p.putBytes("custom_thm", &c, sizeof(c));
    p.end();
}

// ── API: Preferencias ─────────────────────────────────────────
int  vault_get_theme()             { return vault_get_theme_raw(); }
void vault_set_theme(int t)        { vault_set_theme_raw(t); }
int  vault_get_accent()            { return vault_get_accent_raw(); }
void vault_set_accent(int a)       { vault_set_accent_raw(a); }
int  vault_get_lock_minutes()      { return _vault_ready ? _prefs.getInt(VAULT_LOCK_KEY, -1) : -1; }
void vault_set_lock_minutes(int m) { if (_vault_ready) _prefs.putInt(VAULT_LOCK_KEY, m); }

// ── API: Configuración del generador de contraseñas ───────────
struct VaultGenConfig {
    int  len;
    bool upper;
    bool nums;
    bool syms;
    bool ambig;
};

VaultGenConfig vault_get_gen_config() {
    Preferences p;
    p.begin("vprefs", true);
    VaultGenConfig c;
    c.len   = p.getInt("gen_len",   10);
    c.upper = p.getBool("gen_upper", true);
    c.nums  = p.getBool("gen_nums",  true);
    c.syms  = p.getBool("gen_syms",  false);
    c.ambig = p.getBool("gen_ambig", true);
    p.end();
    return c;
}

void vault_set_gen_config(int len, bool upper, bool nums, bool syms, bool ambig) {
    Preferences p;
    p.begin("vprefs", false);
    p.putInt("gen_len",    len);
    p.putBool("gen_upper", upper);
    p.putBool("gen_nums",  nums);
    p.putBool("gen_syms",  syms);
    p.putBool("gen_ambig", ambig);
    p.end();
}

// ── API: Layout de teclado ────────────────────────────────────
// 0=ES-MX, 1=ES-ES, 2=US
int  vault_get_kb_layout() {
    Preferences p; p.begin("vprefs", true);
    int v = p.getInt("kb_layout", 0); p.end(); return v;
}
void vault_set_kb_layout(int l) {
    Preferences p; p.begin("vprefs", false);
    p.putInt("kb_layout", l); p.end();
}
// ── API: Idioma de la interfaz ─────────────────────────────────
// 0=Español, 1=English
int  vault_get_lang() {
    Preferences p; p.begin("vprefs", true);
    int v = p.getInt("lang", 0); p.end(); return v;
}
void vault_set_lang(int l) {
    Preferences p; p.begin("vprefs", false);
    p.putInt("lang", l); p.end();
}
// kb_mode: 0=USB, 1=Bluetooth
int  vault_get_kb_mode() {
    Preferences p; p.begin("vprefs", true);
    int v = p.getInt("kb_mode", 0); p.end(); return v;
}
void vault_set_kb_mode(int m) {
    Preferences p; p.begin("vprefs", false);
    p.putInt("kb_mode", m); p.end();
}
bool vault_get_kb_popup_shown() {
    Preferences p; p.begin("vprefs", true);
    bool v = p.getBool("kb_popup", false); p.end(); return v;
}

// Modo almacenamiento USB (USBMSC) — bandera de arranque, se revisa
// ANTES de cargar vault/LVGL/WiFi/teclado. true = arrancar solo como
// disco USB; se borra al salir (botón en pantalla o desconectar cable).
bool vault_get_usb_msc_mode() {
    Preferences p; p.begin("vprefs", true);
    bool v = p.getBool("usb_msc", false); p.end(); return v;
}
void vault_set_usb_msc_mode(bool on) {
    Preferences p; p.begin("vprefs", false);
    p.putBool("usb_msc", on); p.end();
}

// Preferencia: exponer la SD como solo-lectura en modo USB (default: false,
// lectura-escritura). En solo-lectura, el host puede copiar los backups pero
// no puede sobrescribir ni borrar nada en la tarjeta.
bool vault_get_usb_msc_readonly() {
    Preferences p; p.begin("vprefs", true);
    bool v = p.getBool("usb_msc_ro", false); p.end(); return v;
}
void vault_set_usb_msc_readonly(bool on) {
    Preferences p; p.begin("vprefs", false);
    p.putBool("usb_msc_ro", on); p.end();
}

// ── API: Feedback háptico ──────────────────────────────────────
// Solo guarda la preferencia — la lógica física (vibrar de verdad)
// se conecta cuando exista el módulo de vibración en el hardware.
bool vault_get_haptic_enabled() {
    Preferences p; p.begin("vprefs", true);
    bool v = p.getBool("haptic", true); p.end(); return v; // default: activado
}
void vault_set_haptic_enabled(bool en) {
    Preferences p; p.begin("vprefs", false);
    p.putBool("haptic", en); p.end();
}

// ── API: Onboarding completado ─────────────────────────────────
bool vault_onboarding_done() {
    Preferences p; p.begin("vprefs", true);
    bool v = p.getBool("onboard_ok", false); p.end(); return v;
}
void vault_set_onboarding_done(bool done) {
    Preferences p; p.begin("vprefs", false);
    p.putBool("onboard_ok", done); p.end();
}
void vault_set_kb_popup_shown(bool v) {
    Preferences p; p.begin("vprefs", false);
    p.putBool("kb_popup", v); p.end();
}
int  vault_get_sort() {
    Preferences p; p.begin("vprefs", true);
    int v = p.getInt("sort_mode", 0); p.end(); return v;
}
void vault_set_sort(int s) {
    Preferences p; p.begin("vprefs", false);
    p.putInt("sort_mode", s); p.end();
}
bool vault_get_view_icons() {
    Preferences p; p.begin("vprefs", true);
    bool v = p.getBool("view_icons", false); p.end(); return v;
}
void vault_set_view_icons(bool v) {
    Preferences p; p.begin("vprefs", false);
    p.putBool("view_icons", v); p.end();
}

// ── API: Guardar entrada ──────────────────────────────────────

/**
 * Guarda una cuenta web (tipo WEB).
 * username y password son obligatorios.
 */
// Genera un nombre único basado en el nombre dado, añadiendo sufijo si ya existe
static void _vault_unique_name(const char* base, char* out, int out_len) {
    bool found = false;
    _vaultfs_locate(base, &found);
    if (!found) {
        strncpy(out, base, out_len - 1);
        out[out_len - 1] = '\0';
        return;
    }
    for (int i = 2; i < 999; i++) {
        String candidate = String(base) + " " + String(i);
        _vaultfs_locate(candidate.c_str(), &found);
        if (!found) {
            strncpy(out, candidate.c_str(), out_len - 1);
            out[out_len - 1] = '\0';
            return;
        }
    }
    strncpy(out, base, out_len - 1);
    out[out_len - 1] = '\0';
}

bool vault_save_web(const char* name,
                    const char* username,
                    const char* password,
                    const char* category_id = "none",
                    const char* notes       = "",
                    const char* icon        = "") {
    if (!_vault_ready) return false;
    if (!name || strlen(name) == 0)     return false;
    if (!username || strlen(username) == 0) return false;
    if (!password || strlen(password) == 0) return false;

    // Señuelo: aparenta éxito (UI indistinguible del modo real) pero no
    // persiste nada — el señuelo nunca debe acumular entradas reales.
    if (_vault_is_decoy) { Serial.println("[VAULT] (señuelo) guardado simulado, no persistido"); return true; }

    // Generar nombre único si ya existe (solo al crear, no al editar)
    char unique_name[VAULT_FIELD_LEN] = {0};
    _vault_unique_name(name, unique_name, VAULT_FIELD_LEN);
    name = unique_name;

    // Buscar slot (existente=edición, libre=creación) en una sola pasada
    bool found = false;
    int32_t slot = _vaultfs_locate(name, &found);
    Serial.printf("[VAULT] locate(%s) -> slot=%ld found=%d\n", name, (long)slot, found);
    if (slot < 0) { Serial.println("[VAULT] FALLO: vault lleno, sin slot libre"); return false; }

    VaultEntry prev;
    bool is_new = !found || !_vaultfs_read_record(slot, prev);

    VaultEntry e = {};
    e.valid = true;
    e.type  = VAULT_TYPE_WEB;
    strncpy(e.name, name, VAULT_FIELD_LEN - 1);
    strncpy(e.username, username, VAULT_FIELD_LEN - 1);
    strncpy(e.password, password, VAULT_FIELD_LEN - 1);
    strncpy(e.category_id, category_id ? category_id : "none", VAULT_CAT_LEN - 1);
    strncpy(e.icon, icon ? icon : "", VAULT_FIELD_LEN - 1);
    if (notes) strncpy(e.notes, notes, VAULT_NOTES_LEN - 1);

    e.created_at  = is_new ? (uint32_t)(millis() / 1000) : prev.created_at;
    e.is_favorite = is_new ? false : prev.is_favorite;
    if (!is_new) strncpy(e.totp_secret, prev.totp_secret, VAULT_FIELD_LEN - 1);

    vault_op_busy_begin();
    if (!_vaultfs_write_record(slot, e)) { Serial.printf("[VAULT] FALLO _vaultfs_write_record en slot %ld para %s\n", (long)slot, name); vault_op_busy_end(); return false; }
    vaultfs_index_upsert(slot, e);
    vault_op_busy_end();

    Serial.printf("[VAULT] Web guardada: %s\n", name);
    return true;
}

/**
 * Guarda un secreto flexible (tipo SECRET).
 * secret es obligatorio (PIN, código, contraseña genérica).
 */
bool vault_save_secret(const char* name,
                       const char* secret,
                       const char* category_id = "none",
                       const char* notes       = "",
                       const char* icon        = "") {
    if (!_vault_ready) return false;
    if (!name   || strlen(name)   == 0) return false;
    if (!secret) secret = "";
    // Secreto vacío permitido solo para entradas "solo nota" (nota presente)
    if (strlen(secret) == 0 && (!notes || strlen(notes) == 0)) return false;

    if (_vault_is_decoy) { Serial.println("[VAULT] (señuelo) guardado simulado, no persistido"); return true; }

    bool found = false;
    int32_t slot = _vaultfs_locate(name, &found);
    if (slot < 0) { Serial.println("[VAULT] FALLO: vault lleno, sin slot libre"); return false; }

    VaultEntry prev;
    bool is_new = !found || !_vaultfs_read_record(slot, prev);

    VaultEntry e = {};
    e.valid = true;
    e.type  = VAULT_TYPE_SECRET;
    strncpy(e.name, name, VAULT_FIELD_LEN - 1);
    strncpy(e.secret, secret, VAULT_FIELD_LEN - 1);
    strncpy(e.category_id, category_id ? category_id : "none", VAULT_CAT_LEN - 1);
    strncpy(e.icon, icon ? icon : "", VAULT_FIELD_LEN - 1);
    if (notes) strncpy(e.notes, notes, VAULT_NOTES_LEN - 1);

    e.created_at  = is_new ? (uint32_t)(millis() / 1000) : prev.created_at;
    e.is_favorite = is_new ? false : prev.is_favorite;
    if (!is_new) strncpy(e.totp_secret, prev.totp_secret, VAULT_FIELD_LEN - 1);

    vault_op_busy_begin();
    if (!_vaultfs_write_record(slot, e)) { vault_op_busy_end(); return false; }
    vaultfs_index_upsert(slot, e);
    vault_op_busy_end();

    Serial.printf("[VAULT] Secreto guardado: %s\n", name);
    return true;
}

// ── API: Cargar entrada ───────────────────────────────────────
VaultEntry vault_load(const char* name) {
    VaultEntry e = {};
    e.valid = false;
    if (!_vault_ready || !name) return e;

    bool found = false;
    int32_t slot = _vaultfs_locate(name, &found);
    if (!found) return e;
    _vaultfs_read_record(slot, e);
    return e;
}

// ── API: Borrar entrada ───────────────────────────────────────
bool vault_delete(const char* name) {
    if (!_vault_ready || !name) return false;
    if (_vault_is_decoy) return true; // simula éxito, no hay nada que borrar realmente
    vault_op_busy_begin();
    bool ok = _vaultfs_delete_by_name(name);
    if (ok) vaultfs_index_remove(name);
    vault_op_busy_end();
    return ok;
}

// ── API: Listar entradas ──────────────────────────────────────
// Lee directo del índice en RAM (construido al desbloquear) — sin
// tocar disco ni desencriptar nada en cada llamada.
void vault_list(char out_names[][VAULT_FIELD_LEN], int* out_count,
                int max_count,
                const char* filter_cat  = nullptr,
                int         filter_type = -1) {       // -1 = todos los tipos
    *out_count = 0;
    if (!_vault_ready || !_ram_index_ready) return;

    for (int i = 0; i < _ram_index_count && *out_count < max_count; i++) {
        VaultRamEntry& re = _ram_index[i];
        if (filter_cat && strcmp(filter_cat, CAT_ALL) != 0) {
            const char* cat = strlen(re.category_id) ? re.category_id : "none";
            if (strcmp(cat, filter_cat) != 0) continue;
        }
        if (filter_type >= 0 && (int)re.type != filter_type) continue;
        strncpy(out_names[*out_count], re.name, VAULT_FIELD_LEN - 1);
        out_names[*out_count][VAULT_FIELD_LEN - 1] = '\0';
        (*out_count)++;
    }
}

// Lista con metadata (name+username+type+favorite) — también desde RAM.
struct VaultListMetaItem {
    char name[VAULT_FIELD_LEN];
    char username[VAULT_FIELD_LEN];
    char category_id[VAULT_CAT_LEN];
    uint8_t type;
    bool favorite;
};
int vault_list_meta(VaultListMetaItem* out, int max_count) {
    if (!_vault_ready || !_ram_index_ready) return 0;
    int n = (_ram_index_count < max_count) ? _ram_index_count : max_count;
    for (int i = 0; i < n; i++) {
        strncpy(out[i].name, _ram_index[i].name, VAULT_FIELD_LEN - 1);
        out[i].name[VAULT_FIELD_LEN - 1] = '\0';
        strncpy(out[i].username, _ram_index[i].username, VAULT_FIELD_LEN - 1);
        out[i].username[VAULT_FIELD_LEN - 1] = '\0';
        strncpy(out[i].category_id, _ram_index[i].category_id, VAULT_CAT_LEN - 1);
        out[i].category_id[VAULT_CAT_LEN - 1] = '\0';
        out[i].type = _ram_index[i].type;
        out[i].favorite = _ram_index[i].favorite;
    }
    return n;
}

int vault_count(const char* filter_cat = nullptr, int filter_type = -1) {
    if (!_vault_ready || !_ram_index_ready) return 0;
    if (!filter_cat && filter_type < 0) return _ram_index_count;
    int count = 0;
    for (int i = 0; i < _ram_index_count; i++) {
        VaultRamEntry& re = _ram_index[i];
        if (filter_cat && strcmp(filter_cat, CAT_ALL) != 0) {
            const char* cat = strlen(re.category_id) ? re.category_id : "none";
            if (strcmp(cat, filter_cat) != 0) continue;
        }
        if (filter_type >= 0 && (int)re.type != filter_type) continue;
        count++;
    }
    return count;
}

// Tipo de una entrada sin cargarla completa — desde RAM, sin tocar disco
uint8_t vault_get_type(const char* name) {
    if (!_vault_ready || !name) return VAULT_TYPE_WEB;
    int idx = _ram_index_find(name);
    return (idx >= 0) ? _ram_index[idx].type : VAULT_TYPE_WEB;
}

// ── API: Categorías ───────────────────────────────────────────
void vault_cat_list(VaultCategory* out, int* out_count, int max_count) {
    *out_count = 0;
    _prefs_cat.begin(VAULT_CAT_NAMESPACE, true);
    String idx = _prefs_cat.getString("cat_idx", "");
    int start = 0;
    while (*out_count < max_count) {
        int comma = idx.indexOf(',', start);
        if (comma < 0) break;
        String id_num = idx.substring(start, comma);
        if (id_num.length() > 0) {
            String nk = "c" + id_num + "_name";
            String nm = _prefs_cat.getString(nk.c_str(), "");
            if (nm.length() > 0) {
                VaultCategory& cat = out[*out_count];
                snprintf(cat.id, VAULT_CAT_LEN, "cat_%s", id_num.c_str());
                strncpy(cat.name, nm.c_str(), VAULT_CAT_LEN - 1);
                cat.default_idx = 0;
                if (strncmp(nm.c_str(), "DEFAULT_", 8) == 0) {
                    int n = atoi(nm.c_str() + 8);
                    if (n >= 1 && n <= 5) cat.default_idx = n;
                }
                cat.valid = true;
                (*out_count)++;
            }
        }
        start = comma + 1;
    }
    _prefs_cat.end();
}

bool vault_cat_create(const char* name, char* out_id = nullptr) {
    if (!name || strlen(name) == 0) return false;
    // Verificar si ya existe una categoría con ese nombre — escaneando los
    // slots reales [0, VAULT_MAX_CATS): el contador cat_count quedaba
    // desfasado tras borrar carpetas.
    _prefs_cat.begin(VAULT_CAT_NAMESPACE, true);
    for (int i = 0; i < VAULT_MAX_CATS; i++) {
        String nk = "c" + String(i) + "_name";
        String nm = _prefs_cat.getString(nk.c_str(), "");
        if (nm.length() > 0 && nm.equalsIgnoreCase(name)) {
            _prefs_cat.end();
            return false; // Ya existe
        }
    }
    _prefs_cat.end();
    _prefs_cat.begin(VAULT_CAT_NAMESPACE, false);
    // Tope duro: los buffers de listado se dimensionan a VAULT_MAX_CATS(+6).
    // Contar las carpetas ACTIVAS y reciclar el primer slot libre — el
    // contador monótono anterior nunca liberaba huecos al borrar y al
    // llegar a 16 rechazaba crear para siempre. Reusar un ID es seguro:
    // vault_cat_delete ya movió sus entradas a "none".
    int active = 0, free_slot = -1;
    for (int i = 0; i < VAULT_MAX_CATS; i++) {
        String nk = "c" + String(i) + "_name";
        if (_prefs_cat.getString(nk.c_str(), "").length() > 0) active++;
        else if (free_slot < 0) free_slot = i;
    }
    if (active >= VAULT_MAX_CATS || free_slot < 0) { _prefs_cat.end(); return false; }
    String idx = _prefs_cat.getString("cat_idx", "");
    String nk = "c" + String(free_slot) + "_name";
    _prefs_cat.putString(nk.c_str(), name);
    idx += String(free_slot) + ",";
    _prefs_cat.putString("cat_idx", idx);
    if (out_id) snprintf(out_id, VAULT_CAT_LEN, "cat_%d", free_slot);
    _prefs_cat.end();
    return true;
}

// Crea las categorías predefinidas en NVS si no existen todavía
void vault_cat_init_defaults() {
    _prefs_cat.begin(VAULT_CAT_NAMESPACE, true);
    bool already = _prefs_cat.getBool("defaults_ok", false);
    _prefs_cat.end();
    if (already) return;

    const char* defaults[] = {
        "Finanzas", "Trabajo",
        "Streaming", "Hobby", "Personal"
    };
    for (int i = 0; i < 5; i++) vault_cat_create(defaults[i]);

    // Migrar entradas que tengan cat "Sin categoria" a "none"
    // (por si existía de versiones anteriores)

    _prefs_cat.begin(VAULT_CAT_NAMESPACE, false);
    _prefs_cat.putBool("defaults_ok", true);
    _prefs_cat.end();
}

// Migrar categoría "Sin categoria" de NVS → "none"
void vault_migrate_sin_categoria() {
    // Buscar si existe una cat con nombre "Sin categoria" o "Sin categoría"
    VaultCategory cats[VAULT_MAX_CATS + 6];
    int count = 0;
    vault_cat_list(cats, &count, VAULT_MAX_CATS + 6);
    for (int i = 0; i < count; i++) {
        if (strcasecmp(cats[i].name, "Sin categoria") == 0 ||
            strcasecmp(cats[i].name, "Sin categoría") == 0) {
            // Reasignar todas sus entradas a "none"
            char (*names)[VAULT_FIELD_LEN] = (char(*)[VAULT_FIELD_LEN])
                ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES_FS);
            int ec = 0;
            if (names) {
                vault_list(names, &ec, VAULT_MAX_ENTRIES_FS, cats[i].id);
                for (int j = 0; j < ec; j++) {
                    bool found = false;
                    int32_t slot = _vaultfs_locate(names[j], &found);
                    if (!found) continue;
                    VaultEntry e;
                    if (_vaultfs_read_record(slot, e)) {
                        strncpy(e.category_id, "none", VAULT_CAT_LEN - 1);
                        if (_vaultfs_write_record(slot, e)) vaultfs_index_upsert(slot, e);
                    }
                }
                free(names);
            }
            // Eliminar la categoría de NVS — inline para evitar forward decl
            _prefs_cat.begin(VAULT_CAT_NAMESPACE, false);
            String _nk = "c" + String(cats[i].id + 4) + "_name";
            _prefs_cat.remove(_nk.c_str());
            String _idx = _prefs_cat.getString("cat_idx", "");
            String _token = String(cats[i].id + 4) + ",";
            int _pos = _idx.indexOf(_token);
            if (_pos >= 0) _idx.remove(_pos, _token.length());
            _prefs_cat.putString("cat_idx", _idx);
            _prefs_cat.end();
        }
    }
}

bool vault_cat_rename(const char* cat_id, const char* new_name) {
    if (!cat_id || strncmp(cat_id, "cat_", 4) != 0) return false;
    if (!new_name || strlen(new_name) == 0) return false;
    _prefs_cat.begin(VAULT_CAT_NAMESPACE, false);
    // Rechazar si ya existe OTRA categoría con ese nombre (ignorando may/min).
    // Se permite "renombrar" a lo mismo que ya tenía (p. ej. corregir mayúsculas).
    String my_nk = "c" + String(cat_id + 4) + "_name";
    for (int i = 0; i < VAULT_MAX_CATS; i++) {
        String nk = "c" + String(i) + "_name";
        if (nk == my_nk) continue; // no compararse consigo misma
        String nm = _prefs_cat.getString(nk.c_str(), "");
        if (nm.length() > 0 && nm.equalsIgnoreCase(new_name)) {
            _prefs_cat.end();
            return false; // ya existe otra carpeta con ese nombre
        }
    }
    _prefs_cat.putString(my_nk.c_str(), new_name);
    _prefs_cat.end();
    return true;
}

bool vault_cat_delete(const char* cat_id) {
    if (!cat_id || strncmp(cat_id, "cat_", 4) != 0) return false;
    _prefs_cat.begin(VAULT_CAT_NAMESPACE, false);
    String nk = "c" + String(cat_id + 4) + "_name";
    _prefs_cat.remove(nk.c_str());
    String idx = _prefs_cat.getString("cat_idx", "");
    String token = String(cat_id + 4) + ",";
    int pos = idx.indexOf(token);
    if (pos >= 0) idx.remove(pos, token.length());
    _prefs_cat.putString("cat_idx", idx);
    _prefs_cat.end();

    // Mover entradas de esta cat a "none"
    if (_vault_ready) {
        char (*names)[VAULT_FIELD_LEN] = (char(*)[VAULT_FIELD_LEN])
            ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES_FS);
        if (names) {
            int count = 0;
            vault_list(names, &count, VAULT_MAX_ENTRIES_FS, cat_id);
            for (int i = 0; i < count; i++) {
                bool found = false;
                int32_t slot = _vaultfs_locate(names[i], &found);
                if (!found) continue;
                VaultEntry e;
                if (_vaultfs_read_record(slot, e)) {
                    strncpy(e.category_id, "none", VAULT_CAT_LEN - 1);
                    if (_vaultfs_write_record(slot, e)) vaultfs_index_upsert(slot, e);
                }
            }
            free(names);
        }
    }
    return true;
}

String vault_cat_name(const char* cat_id) {
    if (!cat_id || strlen(cat_id) == 0) return "Sin categoria";
    if (strcmp(cat_id, CAT_ALL)  == 0)  return "Todas";
    if (strcmp(cat_id, CAT_NONE) == 0)  return "Sin categoria";
    if (strncmp(cat_id, "cat_", 4) != 0) return "Sin categoria";
    _prefs_cat.begin(VAULT_CAT_NAMESPACE, true);
    String nk = "c" + String(cat_id + 4) + "_name";
    String nm = _prefs_cat.getString(nk.c_str(), "Sin categoria");
    _prefs_cat.end();
    return nm;
}

bool vault_change_pin(const char* new_pin) {
    if (!_vault_ready) return false;
    if (!vaultfs_init()) return false;

    // 1. Cargar entradas en memoria temporal
    int count = _ram_index_count;
    VaultEntry* buf = (VaultEntry*)ps_malloc(sizeof(VaultEntry) * (count > 0 ? count : 1));
    if (!buf) { Serial.println("[VAULT] Sin PSRAM para cambiar PIN"); return false; }

    bool all_ok = true;
    for (int i = 0; i < count; i++) {
        if (!_vaultfs_read_record(_ram_index[i].slot, buf[i])) { all_ok = false; buf[i].valid = false; }
    }

    // Derivar nueva clave y actualizar valor de control
    _derive_key(new_pin);
    _write_control(_prefs, VAULT_PIN_KEY, _vault_key);
    vault_frp_save_pin(new_pin);

    // 2. Recifrar y guardar entradas con la nueva clave
    for (int i = 0; i < count; i++) {
        if (!buf[i].valid) continue;
        if (!_vaultfs_write_record(_ram_index[i].slot, buf[i])) all_ok = false;
    }

    free(buf);
    Serial.printf("[VAULT] PIN cambiado — %d entradas recifradas (ok=%d)\n", count, all_ok);
    return all_ok;
}

// ── Generador de contraseñas ──────────────────────────────────
void vault_generate_password(char* out, int len,
                              bool upper, bool nums,
                              bool syms, bool no_ambig) {
    const char* lower_all  = "abcdefghijklmnopqrstuvwxyz";
    const char* lower_safe = "abcdefghjkmnpqrstuvwxyz";
    const char* upper_all  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char* upper_safe = "ABCDEFGHJKMNPQRSTUVWXYZ";
    const char* nums_all   = "0123456789";
    const char* nums_safe  = "23456789";
    const char* symbols    = "!@#$%&*";

    char charset[128] = {0};
    int ci = 0;

    const char* lc = no_ambig ? lower_safe : lower_all;
    while (*lc) charset[ci++] = *lc++;

    if (upper) {
        const char* uc = no_ambig ? upper_safe : upper_all;
        while (*uc) charset[ci++] = *uc++;
    }
    if (nums) {
        const char* nc = no_ambig ? nums_safe : nums_all;
        while (*nc) charset[ci++] = *nc++;
    }
    if (syms) {
        const char* sc = symbols;
        while (*sc) charset[ci++] = *sc++;
    }
    if (ci == 0) { strncpy(out, "password", len > 0 ? len : 1); out[len > 0 ? len : 0] = '\0'; return; }

    // Limitar longitud
    if (len < 1) len = 1;
    if (len > 32) len = 32;

    // Muestreo uniforme sin sesgo (rejection sampling)
    uint32_t limit = (UINT32_MAX / (uint32_t)ci) * (uint32_t)ci;
    for (int i = 0; i < len; i++) {
        uint32_t r;
        do { r = esp_random(); } while (r >= limit);
        out[i] = charset[r % ci];
    }
    out[len] = '\0';
}

void vault_factory_reset() {
    // Cerrar _prefs si está abierto — NVS no permite dos handles al mismo namespace
    _prefs.end();

    // Abrir explícitamente el namespace del vault y borrarlo
    Preferences pv;
    pv.begin(VAULT_NVS_NAMESPACE, false);
    pv.clear();
    pv.end();

    // Borrar el vault nuevo (todos los buckets) y, por si quedó algo del
    // backend viejo sin migrar/purgar, también esos restos.
    if (vaultfs_init()) {
        vaultfs_remove_all_buckets();
        _fs_ready = false; // forzar que el próximo vaultfs_init() recree los buckets
    }
    vaultfs_index_clear();
    if (_fs_begin()) {
        fs::File dir = LittleFS.open(ENTRIES_FS_DIR);
        if (dir && dir.isDirectory()) {
            fs::File file = dir.openNextFile();
            while (file) {
                String fpath = file.path();
                file.close();
                LittleFS.remove(fpath.c_str());
                file = dir.openNextFile();
            }
        }
        if (dir) dir.close();
        if (LittleFS.exists(ENTRIES_INDEX_PATH)) LittleFS.remove(ENTRIES_INDEX_PATH);
    }

    // Borrar categorías
    Preferences pc;
    pc.begin(VAULT_CAT_NAMESPACE, false);
    pc.clear();
    pc.end();

    // Borrar preferencias de UI
    Preferences pp;
    pp.begin("vprefs", false);
    pp.clear();
    pp.end();

    // Borrar residuos del vault señuelo (config y categorías). Los writes del
    // señuelo son simulados, pero sus namespaces pueden quedar como residuo.
    Preferences pdv;
    pdv.begin(VAULT_DECOY_NAMESPACE, false);
    pdv.clear();
    pdv.end();
    Preferences pdc;
    pdc.begin(VAULT_DCAT_NAMESPACE, false);
    pdc.clear();
    pdc.end();

    // vsec sí se resetea
    Preferences ps;
    ps.begin(VAULT_SEC_NAMESPACE, false);
    ps.clear();
    ps.end();

    // vfrp NO se borra — Factory Reset Protection persiste
    memset(_vault_key, 0, 32);
    _vault_ready = false;
    Serial.println("[VAULT] Factory reset completado");
}

// ── API: Favoritos ────────────────────────────────────────────
bool vault_set_favorite(const char* name, bool fav) {
    if (!_vault_ready || !name) return false;
    if (_vault_is_decoy) return true; // simula éxito, no persiste
    bool found = false;
    int32_t slot = _vaultfs_locate(name, &found);
    if (!found) return false;
    VaultEntry e;
    if (!_vaultfs_read_record(slot, e)) return false;
    e.is_favorite = fav;
    if (!_vaultfs_write_record(slot, e)) return false;
    vaultfs_index_upsert(slot, e);
    return true;
}

bool vault_get_favorite(const char* name) {
    if (!_vault_ready || !name) return false;
    int idx = _ram_index_find(name);
    return (idx >= 0) ? _ram_index[idx].favorite : false;
}

uint32_t vault_get_created_at(const char* name) {
    if (!_vault_ready || !name) return 0;
    int idx = _ram_index_find(name);
    return (idx >= 0) ? _ram_index[idx].created_at : 0;
}
// Formato de hora: true=24h, false=12h
bool vault_get_time_24h() {
    Preferences p; p.begin("vprefs", true);
    bool v = p.getBool("time24h", true); p.end(); return v;
}
void vault_set_time_24h(bool v) {
    Preferences p; p.begin("vprefs", false);
    p.putBool("time24h", v); p.end();
}

// Offset en minutos respecto a UTC (ej. GMT-6 = -360). El RTC guarda
// hora local; TOTP necesita UTC — este offset la compensa al calcular.
int vault_get_tz_offset_min() {
    Preferences p; p.begin("vprefs", true);
    int v = p.getInt("tz_off", 0); p.end(); return v;
}
void vault_set_tz_offset_min(int v) {
    Preferences p; p.begin("vprefs", false);
    p.putInt("tz_off", v); p.end();
}

bool vault_get_t9_mode() {
    Preferences p; p.begin("vprefs", true);
    bool v = p.getBool("t9mode", false); p.end(); return v;
}
void vault_set_t9_mode(bool v) {
    Preferences p; p.begin("vprefs", false);
    p.putBool("t9mode", v); p.end();
}

// ── API: TOTP (RFC 6238) ─────────────────────────────────────────
int  rtc_get_year();   int rtc_get_month();  int rtc_get_day();
int  rtc_get_hour();   int rtc_get_minute(); int rtc_get_second(); // rtc_manager.h
// Actualiza (o borra si secret="") el campo TOTP de una entrada ya
// existente, preservando el resto de sus datos.
bool vault_set_totp_secret(const char* name, const char* secret) {
    if (!_vault_ready || !name) return false;
    bool found = false;
    int32_t slot = _vaultfs_locate(name, &found);
    if (!found) return false;
    VaultEntry e;
    if (!_vaultfs_read_record(slot, e)) return false;
    strncpy(e.totp_secret, secret ? secret : "", VAULT_FIELD_LEN - 1);
    e.totp_secret[VAULT_FIELD_LEN - 1] = '\0';
    if (!_vaultfs_write_record(slot, e)) return false;
    vaultfs_index_upsert(slot, e);
    return true;
}

// Base32 (RFC 4648) → bytes. Retorna cantidad de bytes decodificados,
// o 0 si el secret está vacío/mal formado.
static size_t _totp_base32_decode(const char* in, uint8_t* out, size_t out_cap) {
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    uint32_t buf = 0;
    int bits = 0;
    size_t out_len = 0;
    for (const char* p = in; *p; p++) {
        char c = *p;
        if (c == '=' || c == ' ') continue;
        if (c >= 'a' && c <= 'z') c -= 32; // tolerar minúsculas
        const char* pos = strchr(alphabet, c);
        if (!pos) continue; // ignora caracteres inválidos
        buf = (buf << 5) | (pos - alphabet);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            if (out_len >= out_cap) break;
            out[out_len++] = (uint8_t)((buf >> bits) & 0xFF);
        }
    }
    return out_len;
}

// Días transcurridos desde 1970-01-01 hasta year-month-day (UTC, sin DST).
static uint32_t _totp_days_since_epoch(int year, int month, int day) {
    static const int cum[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    uint32_t days = 0;
    for (int y = 1970; y < year; y++)
        days += ((y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365);
    days += cum[month - 1];
    if (month > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) days += 1;
    days += (day - 1);
    return days;
}

// Unix time actual (UTC), aplicando el offset de zona horaria — usado
// tanto por TOTP como por Mano Muerta. Única fuente de tiempo del vault.
static uint32_t _vault_unix_now() {
    return _totp_days_since_epoch(rtc_get_year(), rtc_get_month(), rtc_get_day()) * 86400UL
         + rtc_get_hour() * 3600UL + rtc_get_minute() * 60UL + rtc_get_second()
         - vault_get_tz_offset_min() * 60L;
}

// Genera el código TOTP vigente (6 dígitos) para el secret ya guardado
// de `name`. Retorna -1 si la entrada no existe o no tiene TOTP.
// `seconds_left` (opcional) recibe cuánto falta para que cambie el código.
// Auto-test con el vector oficial de RFC 6238: secret ASCII
// "12345678901234567890", T=59s (counter=1) → debe dar 287082.
// Se corre una vez al boot, sin depender de la hora del RTC.
static void _totp_self_test() {
    const uint8_t key[20] = {'1','2','3','4','5','6','7','8','9','0','1','2','3','4','5','6','7','8','9','0'};
    uint8_t counter_be[8] = {0,0,0,0,0,0,0,1};
    uint8_t hmac[20] = {0};
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    int rc = mbedtls_md_hmac(info, key, 20, counter_be, 8, hmac);
    int offset = hmac[19] & 0x0F;
    uint32_t code = ((hmac[offset] & 0x7F) << 24) | ((hmac[offset+1] & 0xFF) << 16) |
                     ((hmac[offset+2] & 0xFF) << 8)  |  (hmac[offset+3] & 0xFF);
    Serial.printf("[TOTP-SELFTEST] rc=%d resultado=%06d (esperado=287082)\n", rc, (int)(code % 1000000UL));
}

int vault_totp_now(const char* name, int* seconds_left) {
    static bool _selftest_done = false;
    if (!_selftest_done) { _selftest_done = true; _totp_self_test(); }
    static uint64_t _totp_last_logged_counter = 0;
    VaultEntry e = vault_load(name);
    if (!e.valid || e.totp_secret[0] == '\0') return -1;

    uint8_t key[32];
    size_t key_len = _totp_base32_decode(e.totp_secret, key, sizeof(key));
    uint64_t counter_now = _vault_unix_now() / 30;
    bool do_log = (counter_now != _totp_last_logged_counter);
    if (key_len == 0) return -1;

    uint32_t unix_time = _vault_unix_now();
    uint64_t counter = unix_time / 30;
    if (seconds_left) *seconds_left = 30 - (unix_time % 30);

    uint8_t counter_be[8];
    for (int i = 7; i >= 0; i--) { counter_be[i] = counter & 0xFF; counter >>= 8; }

    uint8_t hmac[20] = {0};
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
    const mbedtls_md_info_t* sha1_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    int hmac_rc = -1;
    if (sha1_info) hmac_rc = mbedtls_md_hmac(sha1_info, key, key_len, counter_be, 8, hmac);
    if (do_log) {
        Serial.printf("[TOTP] sha1_info=%s hmac_rc=%d\n", sha1_info ? "OK" : "NULL(no disponible)", hmac_rc);
    }
    mbedtls_md_free(&ctx);

    int offset = hmac[19] & 0x0F;
    uint32_t code = ((hmac[offset] & 0x7F) << 24) | ((hmac[offset+1] & 0xFF) << 16) |
                     ((hmac[offset+2] & 0xFF) << 8)  |  (hmac[offset+3] & 0xFF);
    int result = (int)(code % 1000000UL);
    if (do_log) {
        // No se registra el secreto, la clave ni el código: material sensible
        // no debe salir por el puerto serie. Solo un latido de diagnóstico.
        Serial.printf("[TOTP] generado (counter=%llu)\n",
                      (unsigned long long)(unix_time/30));
        _totp_last_logged_counter = counter_now;
    }
    return result;
}

// ── API: Mano Muerta (dead man's switch) ─────────────────────────
// Si pasan N días sin un desbloqueo real exitoso, el vault se
// restablece de fábrica automáticamente — pensado para el escenario
// de "perdí/olvidé el dispositivo durante mucho tiempo". Cuenta
// SOLO desbloqueos reales (no el vault señuelo), y usa el RTC como
// fuente de tiempo, así que sigue contando incluso apagado (el RTC
// vive en su propio dominio de energía).
//
// 0 = desactivado (default). El plazo es configurable, sin default
// fijo — el usuario decide según su propio riesgo/uso.
int vault_get_deadman_days() {
    Preferences p; p.begin("vprefs", true);
    int v = p.getInt("dm_days", 0); p.end(); return v;
}
void vault_set_deadman_days(int days) {
    Preferences p; p.begin("vprefs", false);
    p.putInt("dm_days", days); p.end();
}

static uint32_t _vault_get_last_unlock_ts() {
    Preferences p; p.begin("vprefs", true);
    uint32_t v = p.getULong("dm_last", 0); p.end(); return v;
}
static void _vault_set_last_unlock_ts(uint32_t ts) {
    Preferences p; p.begin("vprefs", false);
    p.putULong("dm_last", ts); p.end();
}

// Llamar UNA vez en cada boot, ANTES de mostrar lock/PIN screen —
// necesita el RTC ya inicializado, no necesita el vault desbloqueado.
// No hace nada si está desactivado o si nunca hubo un desbloqueo
// registrado todavía (dispositivo recién configurado).
void vault_deadman_check() {
    int days = vault_get_deadman_days();
    if (days <= 0) return;

    uint32_t last = _vault_get_last_unlock_ts();
    if (last == 0) return; // nunca se registró un desbloqueo — no aplica aún

    uint32_t now = _vault_unix_now();
    if (now < last) return; // RTC desincronizado hacia atrás — no arriesgar un borrado falso

    uint32_t elapsed_days = (now - last) / 86400UL;
    if (elapsed_days >= (uint32_t)days) {
        Serial.printf("[DEADMAN] %u dias sin desbloqueo (limite=%d) — factory reset\n",
                      (unsigned)elapsed_days, days);
        vault_factory_reset();
    }
}

// ── API: Sensibilidad del Snatch Lock (bloqueo por arrebato) ─────
// 0 = desactivado (default), 1 = baja, 2 = media, 3 = alta.
int vault_get_snatch_level() {
    Preferences p; p.begin("vprefs", true);
    int v = p.getInt("snatch_lvl", 0); p.end(); return v;
}
void vault_set_snatch_level(int level) {
    if (level < 0) level = 0;
    if (level > 3) level = 3;
    Preferences p; p.begin("vprefs", false);
    p.putInt("snatch_lvl", level); p.end();
}
