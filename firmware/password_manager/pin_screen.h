#pragma once
// =============================================================
//  pin_screen.h — PIN screen fiel a la referencia
// =============================================================

#include <lvgl.h>
#include "vault.h"
#include "colors.h"
#include "fonts.h"
#include "i18n.h"
#include "vibro_manager.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declarations internas
void pin_screen_show();
void pin_screen_verify(void(*on_correct)());
static void _show_frp_screen();

// Forward declaration — definida en ui_main.h
void ui_main_prepare_lock();
void power_deep_sleep();
void ui_statusbar_init();
void ui_statusbar_set_visible(bool visible);
void ui_statusbar_tick(bool force);
void power_set_locked(bool locked); // definida en power_manager.h
bool onboarding_in_progress();

// PIN_DIGITS ahora viene de vault.h (único punto de cambio)
#define SESSION_TIMEOUT_MS  120000UL
#define PIN_W               240
#define PIN_H               320

typedef void (*pin_unlock_cb_t)(bool success);

// ── Estado ────────────────────────────────────────────────────
static char            _pin_buf[PIN_DIGITS + 1];

static char            _unlock_pin_copy[PIN_DIGITS + 1] = {0};
static volatile bool   _unlock_task_done = false;
static volatile bool   _unlock_task_ok   = false;
static int             _unlock_fails_reserved = 0;
static lv_timer_t*     _unlock_poll_timer = nullptr;
static lv_obj_t*       _unlock_fade_scr   = nullptr; // Overlay "Descifrando..."
static bool            _unlock_in_progress = false; // Evita llamadas superpuestas durante el desbloqueo

static void _unlock_task_fn(void* pv) {
    // Desactivar temporalmente Core 0 WDT durante PBKDF2
    disableCore0WDT();
    bool ok = vault_init(_unlock_pin_copy);
    enableCore0WDT();
    memset(_unlock_pin_copy, 0, sizeof(_unlock_pin_copy));
    _unlock_task_ok   = ok;
    _unlock_task_done = true;
    vTaskDelete(nullptr);
}

static int             _pin_len        = 0;
static int             _attempts       = 0;
static uint32_t        _last_activity  = 0;
static bool            _unlock_pending = false;
static pin_unlock_cb_t _unlock_cb      = nullptr;

static lv_obj_t* _pin_dots[PIN_DIGITS];
static lv_obj_t* _pin_status  = nullptr;
static lv_obj_t* _pin_keys[12];

// ── Colores según tema ────────────────────────────────────────
static lv_color_t _P_BG, _P_FG, _P_MUTED, _P_KEY_BORDER, _P_CARD;
static lv_color_t _P_OK_BG, _P_OK_FG, _P_DANGER, _P_ACCENT;

static void _pin_load_theme() {
    int theme = vault_get_theme();  // lee de vprefs, no necesita vault abierto
    int base = (theme == THEME_PERSONALIZADO) ? vault_get_custom_theme().base : theme;
    if (base == 0) {  // oscuro
        _P_BG         = lv_color_hex(0x1C1C1E);
        _P_FG         = lv_color_hex(0xFFFFFF);
        _P_MUTED      = lv_color_hex(0x8E8E93);
        _P_KEY_BORDER = lv_color_hex(0x3A3A3C);
        _P_CARD       = lv_color_hex(0x2C2C2E);
        _P_OK_BG      = lv_color_hex(0xE7EBF1);
        _P_OK_FG      = lv_color_hex(0x1C1C1E);
    } else {       // claro
        _P_BG         = lv_color_hex(0xE7EBF1);
        _P_FG         = lv_color_hex(0x000000);
        _P_MUTED      = lv_color_hex(0x666666);
        _P_KEY_BORDER = lv_color_hex(0x999999);
        _P_CARD       = lv_color_hex(0xFFFFFF);
        _P_OK_BG      = lv_color_hex(0x1C1C1E);
        _P_OK_FG      = lv_color_hex(0xE7EBF1);
    }
    _P_DANGER = lv_color_hex(0xEE3333);

    if (theme == THEME_PERSONALIZADO) {
        VaultCustomTheme ct = vault_get_custom_theme();
        _P_BG = hsv_to_lv_color(ct.bg_h, ct.bg_s, base == 0 ? 16 : 94);
        _P_CARD = hsv_to_lv_color(ct.card_h, ct.card_s, base == 0 ? 24 : 100);
        _P_FG = color_is_light(_P_BG) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
        _P_ACCENT = hsv_to_lv_color(ct.acc_h, ct.acc_s, 92);
    } else {
        int acc = vault_get_accent();
        if (acc == ACCENT_AUTO) {
            _P_ACCENT = _P_OK_BG; // contraste invertido, igual patrón que C_BTN_BG
        } else {
            if (acc < 0 || acc >= ACCENT_COUNT) acc = 0;
            _P_ACCENT = lv_color_hex(ACCENT_PALETTE[acc]);
        }
    }

    ui_statusbar_init();
    ui_statusbar_set_visible(false); // sólo la pantalla de lista muestra la barra de estado
    ui_statusbar_tick(true);
}

// ── Helpers ───────────────────────────────────────────────────
static void _update_dots() {
    for (int i = 0; i < PIN_DIGITS; i++) {
        if (!_pin_dots[i]) continue;
        if (i < _pin_len) {
            // Relleno azul
            lv_obj_set_style_bg_color(_pin_dots[i], _P_ACCENT, 0);
            lv_obj_set_style_border_color(_pin_dots[i], _P_ACCENT, 0);
        } else {
            // Solo aro gris
            lv_obj_set_style_bg_color(_pin_dots[i], _P_BG, 0);
            lv_obj_set_style_border_color(_pin_dots[i], _P_KEY_BORDER, 0);
        }
    }
}

static void _set_status(const char* msg, bool err = false) {
    if (!_pin_status) return;
    lv_label_set_text(_pin_status, msg);
    lv_obj_set_style_text_color(_pin_status,
        err ? _P_DANGER : _P_MUTED, 0);
}

static void _reset_pin() {
    _pin_len = 0;
    memset(_pin_buf, 0, sizeof(_pin_buf));
    _update_dots();
}

static void _set_keys_enabled(bool en) {
    for (int i = 0; i < 12; i++) {
        if (!_pin_keys[i]) continue;
        if (en) lv_obj_clear_state(_pin_keys[i], LV_STATE_DISABLED);
        else    lv_obj_add_state(_pin_keys[i],   LV_STATE_DISABLED);
    }
}

// ── Contador reset automático vault señuelo ───────────────────
static uint32_t _decoy_reset_start  = 0;
static bool     _decoy_reset_active = false;
#define DECOY_RESET_MS  (30UL * 60UL * 1000UL) // 30 minutos

static void decoy_reset_start() {
    if (!_decoy_reset_active) {
        _decoy_reset_active = true;
        _decoy_reset_start  = millis();
        Serial.println("[DECOY] Contador reset iniciado");
    }
}

static void decoy_reset_cancel() {
    _decoy_reset_active = false;
    _decoy_reset_start  = 0;
    Serial.println("[DECOY] Contador reset cancelado");
}

static void _try_unlock() {
    if (_unlock_in_progress) return; // ya hay un intento en curso — ignorar el disparo duplicado
    _unlock_in_progress = true;

    lv_refr_now(nullptr);
    lv_obj_t* fade = lv_obj_create(lv_scr_act());
    _unlock_fade_scr = fade; // para poder quitarlo desde el timer de resultado, éxito o fallo
    lv_obj_set_size(fade, 240, 320); lv_obj_set_pos(fade, 0, 0);
    lv_obj_set_style_bg_color(fade, _P_BG, 0); // fondo del tema/paleta activos, no negro fijo
    lv_obj_set_style_bg_opa(fade, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fade, 0, 0);
    lv_obj_clear_flag(fade, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(fade, LV_OBJ_FLAG_CLICKABLE);

    // Candado central en pantalla de desbloqueo
    lv_obj_t* ico = lv_label_create(fade);
    lv_label_set_text(ico, ICO_LOCK);
    lv_obj_set_style_text_font(ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(ico, _P_ACCENT, 0);
    lv_obj_align(ico, LV_ALIGN_CENTER, 0, -46);

    lv_obj_t* spin = lv_spinner_create(fade, 900, 90);
    lv_obj_set_size(spin, 34, 34);
    lv_obj_set_style_arc_color(spin, lv_color_hex(0x3A3A3C), LV_PART_MAIN);
    lv_obj_set_style_arc_color(spin, _P_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spin, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spin, 3, LV_PART_INDICATOR);
    lv_obj_align(spin, LV_ALIGN_CENTER, 0, 4);

    // Texto en _P_FG: ya es blanco o negro según lo que contraste bien
    // con _P_BG (ambos calculados arriba para el tema/paleta activos).
    lv_obj_t* msg = lv_label_create(fade);
    lv_label_set_text(msg, t(K_DESCIFRANDO_BOVEDA));
    lv_obj_set_style_text_font(msg, FONT_SM, 0);
    lv_obj_set_style_text_color(msg, _P_FG, 0);
    lv_obj_set_width(msg, 200);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 42);

    lv_refr_now(nullptr);

    _unlock_fails_reserved = vault_get_fail_count() + 1;
    vault_set_fail_count(_unlock_fails_reserved);

    strncpy(_unlock_pin_copy, _pin_buf, sizeof(_unlock_pin_copy) - 1);
    _reset_pin();

    _unlock_task_done = false;
    _unlock_task_ok   = false;
    // Derivación de clave en tarea separada Core 0
    xTaskCreatePinnedToCore(_unlock_task_fn, "vault_unlock", 16384, nullptr, 1, nullptr, 0);

    if (_unlock_poll_timer) lv_timer_del(_unlock_poll_timer);
    _unlock_poll_timer = lv_timer_create([](lv_timer_t* tm){
        if (!_unlock_task_done) return;
        lv_timer_del(tm);
        _unlock_poll_timer = nullptr;

        _unlock_in_progress = false;

        bool ok = _unlock_task_ok;
        int fails = _unlock_fails_reserved;
        if (ok) {
            Serial.println("[PIN] Correcto — reseteando contadores");
            vault_reset_fail_count();
            _attempts = 0;
            // Cancelar contador señuelo si estaba activo
            if (_decoy_reset_active) decoy_reset_cancel();
            // Si es vault señuelo — iniciar contador de reset
            if (vault_is_decoy()) decoy_reset_start();
            _set_status(t(K_ACCESO_CONCEDIDO));
            _unlock_pending = true;
        } else {
            if (_unlock_fade_scr && lv_obj_is_valid(_unlock_fade_scr)) lv_obj_del(_unlock_fade_scr);
            _unlock_fade_scr = nullptr;

            Serial.printf("[PIN] Incorrecto — fail_count=%d\n", fails);
            _attempts = fails;

        if (fails >= 5) {
            Serial.println("[PIN] Limite alcanzado — factory reset");
            vault_factory_reset();
            Serial.println("[PIN] Factory reset completado — reiniciando");
            delay(500);
            ESP.restart();
        } else if (fails == 4) {
            _set_status(t(K_ULTIMO_INTENTO), true);
        } else {
            char msg[40];
            int remaining = 5 - fails;
            snprintf(msg, sizeof(msg), t(K_INCORRECTO_QUEDAN_FMT), remaining);
            _set_status(msg, true);
        }
    }
    }, 40, nullptr);
}

static const char* _kstr[] = {
    "1","2","3","4","5","6","7","8","9","DEL","0","OK"
};

static void _cb_key(lv_event_t* e) {
    if (_unlock_pending) return;
    vibro_pulse();
    _last_activity = millis();
    const char* k = (const char*)lv_event_get_user_data(e);
    if (!k) return;

    if (k[0] == 'D') {
        if (_pin_len > 0) {
            _pin_buf[--_pin_len] = '\0';
            _update_dots();
            _set_status("");
        }
    } else if (k[0] == 'O') {
        if (_pin_len == PIN_DIGITS) _try_unlock();
    } else {
        if (_pin_len < PIN_DIGITS) {
            _pin_buf[_pin_len++] = k[0];
            _pin_buf[_pin_len]   = '\0';
            _update_dots();
            _set_status("");
            if (_pin_len == PIN_DIGITS) _try_unlock();
        }
    }
}

static int _frp_attempts = 0;
static char _frp_buf[PIN_DIGITS + 1] = {0};
static int  _frp_len = 0;
static lv_obj_t* _frp_dots[PIN_DIGITS] = {nullptr};
static lv_obj_t* _frp_status = nullptr;

static void _show_frp_screen() {
    _frp_attempts = vault_frp_get_attempts();  // recuperar intentos ya gastados
    _frp_len = 0;
    memset(_frp_buf, 0, sizeof(_frp_buf));

    _pin_load_theme();
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, PIN_W, PIN_H);
    lv_obj_set_style_bg_color(scr, _P_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Ícono candado
    lv_obj_t* lock_ico = lv_label_create(scr);
    lv_label_set_text(lock_ico, ICO_LOCK);
    lv_obj_set_style_text_font(lock_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(lock_ico, _P_ACCENT, 0);
    lv_obj_align(lock_ico, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, t(K_VERIFICACION_SEGURIDAD));
    lv_obj_set_style_text_color(title, _P_FG, 0);
    lv_obj_set_style_text_font(title, FONT_HDR, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, t(K_INGRESA_PIN_ANTERIOR));
    lv_obj_set_style_text_color(sub, _P_MUTED, 0);
    lv_obj_set_style_text_font(sub, FONT_SM, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 62);

    // Dots
    int dot_d = 14, dot_gap = 10;
    int dot_tot = PIN_DIGITS * dot_d + (PIN_DIGITS - 1) * dot_gap;
    int dot_x0 = (PIN_W - dot_tot) / 2;
    int dot_y = 88;
    for (int i = 0; i < PIN_DIGITS; i++) {
        _frp_dots[i] = lv_obj_create(scr);
        lv_obj_set_size(_frp_dots[i], dot_d, dot_d);
        lv_obj_set_pos(_frp_dots[i], dot_x0 + i * (dot_d + dot_gap), dot_y);
        lv_obj_set_style_radius(_frp_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(_frp_dots[i], _P_BG, 0);
        lv_obj_set_style_bg_opa(_frp_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(_frp_dots[i], _P_KEY_BORDER, 0);
        lv_obj_set_style_border_width(_frp_dots[i], 2, 0);
        lv_obj_set_style_pad_all(_frp_dots[i], 0, 0);
    }

    _frp_status = lv_label_create(scr);
    lv_label_set_text(_frp_status, "");
    lv_obj_set_style_text_color(_frp_status, _P_DANGER, 0);
    lv_obj_set_style_text_font(_frp_status, FONT_SM, 0);
    lv_obj_align(_frp_status, LV_ALIGN_TOP_MID, 0, dot_y + dot_d + 6);

    // Grid
    int key_d = 43, gap = 8;
    int gw = 3 * key_d + 2 * gap;
    int gx = (PIN_W - gw) / 2;
    int gy = 120;

    for (int i = 0; i < 12; i++) {
        if (i == 9) continue;
        bool is_ok = (i == 11);
        int col = i % 3, row_i = i / 3;
        lv_obj_t* btn = lv_btn_create(scr);
        lv_obj_set_size(btn, key_d, key_d);
        lv_obj_set_pos(btn, gx + col * (key_d + gap), gy + row_i * (key_d + gap));
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, _P_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, _P_ACCENT, LV_STATE_PRESSED);

        lv_obj_t* lbl = lv_label_create(btn);
        if (is_ok) {
            lv_label_set_text(lbl, ICO_CHECK);
            lv_obj_set_style_text_color(lbl, _P_ACCENT, 0);
            lv_obj_set_style_text_font(lbl, FONT_ICON, 0);
        } else {
            lv_label_set_text(lbl, _kstr[i]);
            lv_obj_set_style_text_color(lbl, _P_FG, 0);
            lv_obj_set_style_text_font(lbl, FONT_HDR_LG, 0);
        }
        lv_obj_center(lbl);

        static const char* frp_kvals[12];
        frp_kvals[i] = _kstr[i];
        lv_obj_add_event_cb(btn, [](lv_event_t* e){
            vibro_pulse();
            const char* k = (const char*)lv_event_get_user_data(e);
            if (k[0] == 'O') {
                if (_frp_len < PIN_DIGITS) return;
                // Incrementar y persistir el contador ANTES de verificar —
                // un corte de energía a mitad de intento no regala reintentos.
                int att = vault_frp_get_attempts() + 1;
                vault_frp_set_attempts(att);
                if (vault_frp_verify(_frp_buf)) {
                    Serial.println("[FRP] PIN correcto — reiniciando");
                    vault_frp_reset_attempts();
                    _frp_len = 0; memset(_frp_buf, 0, sizeof(_frp_buf));
                    Preferences p; p.begin("vfrp", false);
                    p.putBool("frp_ok", true); p.end();
                    delay(300);
                    ESP.restart();
                } else {
                    _frp_attempts = att;
                    _frp_len = 0; memset(_frp_buf, 0, sizeof(_frp_buf));
                    for (int j = 0; j < PIN_DIGITS; j++)
                        lv_obj_set_style_bg_color(_frp_dots[j], _P_BG, 0);
                    if (att >= 3) {
                        vault_frp_lock_permanent();
                        lv_label_set_text(_frp_status, t(K_DISPOSITIVO_BLOQUEADO));
                        delay(1500);
                        ESP.restart();
                    } else {
                        char msg[40];
                        snprintf(msg, sizeof(msg), t(K_INCORRECTO_FRP_FMT), att);
                        lv_label_set_text(_frp_status, msg);
                    }
                }
            } else if (k[0] == 'D') {
                if (_frp_len > 0) {
                    _frp_buf[--_frp_len] = '\0';
                    lv_obj_set_style_bg_color(_frp_dots[_frp_len], _P_BG, 0);
                    lv_obj_set_style_border_color(_frp_dots[_frp_len], lv_color_hex(0x3A3A3C), 0);
                }
            } else {
                if (_frp_len < PIN_DIGITS) {
                    _frp_buf[_frp_len] = k[0]; _frp_len++;
                    _frp_buf[_frp_len] = '\0';
                    lv_obj_set_style_bg_color(_frp_dots[_frp_len - 1], _P_ACCENT, 0);
                    lv_obj_set_style_border_color(_frp_dots[_frp_len - 1], _P_ACCENT, 0);
                }
            }
        }, LV_EVENT_CLICKED, (void*)frp_kvals[i]);
    }

    // Botón borrar
    lv_obj_t* btn_del = lv_btn_create(scr);
    lv_obj_set_size(btn_del, key_d, key_d);
    lv_obj_set_pos(btn_del, gx, gy + 3 * (key_d + gap));
    lv_obj_set_style_bg_color(btn_del, _P_CARD, 0);
    lv_obj_set_style_bg_opa(btn_del, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_del, 0, 0);
    lv_obj_set_style_radius(btn_del, 14, 0);
    lv_obj_set_style_shadow_width(btn_del, 0, 0);
    lv_obj_set_style_bg_color(btn_del, _P_ACCENT, LV_STATE_PRESSED);
    lv_obj_t* del_lbl = lv_label_create(btn_del);
    lv_label_set_text(del_lbl, ICO_BACKSPACE);
    lv_obj_set_style_text_color(del_lbl, _P_FG, 0);
    lv_obj_set_style_text_font(del_lbl, FONT_ICON, 0);
    lv_obj_center(del_lbl);
    lv_obj_add_event_cb(btn_del, [](lv_event_t* e){
        vibro_pulse();
        if (_frp_len > 0) {
            _frp_buf[--_frp_len] = '\0';
            lv_obj_set_style_bg_color(_frp_dots[_frp_len], _P_BG, 0);
            lv_obj_set_style_border_color(_frp_dots[_frp_len], lv_color_hex(0x3A3A3C), 0);
        }
    }, LV_EVENT_CLICKED, nullptr);

    lv_scr_load(scr);
}

void pin_screen_init(pin_unlock_cb_t cb) { _unlock_cb = cb; }

void pin_screen_show() {
    power_set_locked(true);
    _reset_pin();
    _unlock_pending = false;

    // Verificar bloqueo permanente por FRP
    if (vault_frp_is_locked_permanent()) {
        // Pantalla de bloqueo permanente
        _pin_load_theme();
        lv_obj_t* scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(scr, _P_BG, 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_t* lbl = lv_label_create(scr);
        lv_label_set_text(lbl, t(K_DISPOSITIVO_BLOQUEADO_PERM));
        lv_obj_set_style_text_color(lbl, _P_DANGER, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        lv_obj_t* old = lv_scr_act(); lv_scr_load(scr); if (old && old != scr) lv_obj_del(old);
        return;
    }

    // Verificar si viene de factory reset — pedir PIN anterior (FRP)
    if (vault_frp_is_set()) {
        Preferences _tmp;
        _tmp.begin(VAULT_NVS_NAMESPACE, true);
        bool has_pin = _tmp.getBytesLength(VAULT_PIN_KEY) == VAULT_CTRL_LEN;
        _tmp.end();
        if (!has_pin) {
            // Verificar si ya se autenticó con FRP
            Preferences _frp_tmp;
            _frp_tmp.begin("vfrp", true);
            bool frp_ok = _frp_tmp.getBool("frp_ok", false);
            _frp_tmp.end();
            if (!frp_ok) {
                // Factory reset ocurrió — mostrar pantalla FRP
                _show_frp_screen();
                return;
            } else {
                // FRP ya verificado — limpiar bandera y mostrar PIN normal
                Preferences _frp_clear;
                _frp_clear.begin("vfrp", false);
                _frp_clear.remove("frp_ok");
                _frp_clear.end();
            }
        }
    }

    _attempts = vault_get_fail_count();
    _unlock_pending = false;
    _last_activity  = millis();
    memset(_pin_dots, 0, sizeof(_pin_dots));
    memset(_pin_keys, 0, sizeof(_pin_keys));
    _pin_status  = nullptr;

    _pin_load_theme();

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, PIN_W, PIN_H);
    lv_obj_set_style_bg_color(scr, _P_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── Ícono candado ─────────────────────────────────────────
    lv_obj_t* lock_ico = lv_label_create(scr);
    lv_label_set_text(lock_ico, ICO_LOCK);
    lv_obj_set_style_text_font(lock_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(lock_ico, _P_ACCENT, 0);
    lv_obj_align(lock_ico, LV_ALIGN_TOP_MID, 0, 16);

    // ── Título ────────────────────────────────────────────────
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, t(K_DISPOSITIVO_BLOQUEADO));
    lv_obj_set_style_text_color(title, _P_FG, 0);
    lv_obj_set_style_text_font(title, FONT_HDR, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    // ── Subtítulo ─────────────────────────────────────────────
    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, t(K_INGRESA_PIN_CONTINUAR));
    lv_obj_set_style_text_color(sub, _P_MUTED, 0);
    lv_obj_set_style_text_font(sub, FONT_SM, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 62);

    // ── Dots ──────────────────────────────────────────────────
    int dot_d   = 14;
    int dot_gap = 10;
    int dot_tot = PIN_DIGITS * dot_d + (PIN_DIGITS - 1) * dot_gap;
    int dot_x0  = (PIN_W - dot_tot) / 2;
    int dot_y   = 88;

    for (int i = 0; i < PIN_DIGITS; i++) {
        _pin_dots[i] = lv_obj_create(scr);
        lv_obj_set_size(_pin_dots[i], dot_d, dot_d);
        lv_obj_set_pos(_pin_dots[i], dot_x0 + i * (dot_d + dot_gap), dot_y);
        lv_obj_set_style_radius(_pin_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(_pin_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(_pin_dots[i], 2, 0);
        lv_obj_set_style_pad_all(_pin_dots[i], 0, 0);
    }
    _update_dots();

    // ── Status ────────────────────────────────────────────────
    _pin_status = lv_label_create(scr);
    lv_label_set_text(_pin_status, "");
    lv_obj_set_style_text_color(_pin_status, _P_MUTED, 0);
    lv_obj_set_style_text_font(_pin_status, FONT_SM, 0);
    lv_obj_align(_pin_status, LV_ALIGN_TOP_MID, 0, dot_y + dot_d + 6);

    // ── Grid 3×4 ─────────────────────────────────────────────
    // Espacio disponible: 320 - 120 (header) - 4px margen = 196px
    // 4 filas * key_d + 3 * gap = 196 → key_d=46, gap=6
    int key_d = 43;
    int gap   = 8;
    int gw    = 3 * key_d + 2 * gap;
    int gx    = (PIN_W - gw) / 2;
    int gy    = 120;

    for (int i = 0; i < 12; i++) {
        int col = i % 3;
        int row = i / 3;
        int x   = gx + col * (key_d + gap);
        int y   = gy + row * (key_d + gap);

        bool is_del = (i == 9);
        bool is_ok  = (i == 11);

        lv_obj_t* btn = lv_btn_create(scr);
        lv_obj_set_size(btn, key_d, key_d);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, _P_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);

        // Flash azul al presionar
        lv_obj_set_style_bg_color(btn, _P_ACCENT, LV_STATE_PRESSED);

        lv_obj_t* lbl = lv_label_create(btn);
        if (is_del) {
            lv_label_set_text(lbl, ICO_BACKSPACE);
            lv_obj_set_style_text_color(lbl, _P_FG, 0);
            lv_obj_set_style_text_font(lbl, FONT_ICON, 0);
        } else if (is_ok) {
            lv_label_set_text(lbl, ICO_CHECK);
            lv_obj_set_style_text_color(lbl, _P_ACCENT, 0);
            lv_obj_set_style_text_font(lbl, FONT_ICON, 0);
        } else {
            lv_label_set_text(lbl, _kstr[i]);
            lv_obj_set_style_text_color(lbl, _P_FG, 0);
            lv_obj_set_style_text_font(lbl, FONT_HDR_LG, 0);
        }
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, _cb_key, LV_EVENT_CLICKED, (void*)_kstr[i]);
        _pin_keys[i] = btn;
    }

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 50, true);
}

void pin_screen_activity() { _last_activity = millis(); }

void pin_screen_tick() {
    uint32_t now = millis();

    if (_unlock_pending) {
        _unlock_pending = false;
        if (_unlock_cb) _unlock_cb(true);
        return;
    }

    if (vault_is_ready() && !onboarding_in_progress() && !vault_is_op_busy()) {
        // (el guard de op busy evita que el timeout de inactividad bloquee y
        // duerma el dispositivo a la mitad de un import/export largo)
        int lock_min = vault_get_lock_minutes();
        if (lock_min == 0) return; // Nunca — no bloquear
        uint32_t timeout_ms;
        if (lock_min < 0) timeout_ms = 30000UL;      // 30 seg
        else              timeout_ms = (uint32_t)lock_min * 60000UL;
        if (now - _last_activity > timeout_ms) {
            ui_main_prepare_lock();
            vault_lock();
            power_deep_sleep();
        }
    }

    // Contador reset automático vault señuelo
    if (_decoy_reset_active && (now - _decoy_reset_start >= (uint32_t)(vault_decoy_get_reset_time() * 60UL * 1000UL))) {
        Serial.println("[DECOY] Tiempo agotado — factory reset");
        _decoy_reset_active = false;
        vault_factory_reset();
        delay(500);
        ESP.restart();
    }
}

// ── Pantalla PIN de verificación (sin cambio de pantalla) ────
// Muestra overlay con teclado numérico sobre la pantalla actual
// on_correct: callback si el PIN es correcto (el overlay se cierra antes)
static void(*_pin_verify_cb)() = nullptr;
static char _pin_verify_buf[PIN_DIGITS + 1] = {0};
static int  _pin_verify_len = 0;
static lv_obj_t* _pin_verify_dots[PIN_DIGITS] = {nullptr};
static lv_obj_t* _pin_verify_status = nullptr;

static void _pin_verify_update_dots() {
    for (int i = 0; i < PIN_DIGITS; i++) {
        if (!_pin_verify_dots[i] || !lv_obj_is_valid(_pin_verify_dots[i])) continue;
        bool filled = i < _pin_verify_len;
        lv_obj_set_style_bg_color(_pin_verify_dots[i], filled ? _P_ACCENT : lv_color_hex(0x444444), 0);
        lv_obj_set_style_border_color(_pin_verify_dots[i], filled ? _P_ACCENT : lv_color_hex(0x666666), 0);
    }
}

void pin_screen_verify(void(*on_correct)()) {
    _pin_verify_cb  = on_correct;
    _pin_verify_len = 0;
    memset(_pin_verify_buf, 0, sizeof(_pin_verify_buf));

    _pin_load_theme();

    lv_obj_t* overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, PIN_W, PIN_H); lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, _P_BG, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    // ✗ cancelar
    lv_obj_t* btn_cx = lv_btn_create(overlay);
    lv_obj_set_size(btn_cx, 44, 44); lv_obj_set_pos(btn_cx, PIN_W - 44, 0);
    lv_obj_set_style_bg_opa(btn_cx, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_cx, 0, 0); lv_obj_set_style_border_width(btn_cx, 0, 0);
    lv_obj_t* cx_ico = lv_label_create(btn_cx);
    lv_label_set_text(cx_ico, ICO_CLOSE); lv_obj_set_style_text_font(cx_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(cx_ico, _P_MUTED, 0); lv_obj_center(cx_ico);
    lv_obj_add_event_cb(btn_cx, [](lv_event_t* e){
        vibro_pulse();
        lv_obj_t* ov = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_del(ov);
        lv_obj_clean(lv_layer_top());
    }, LV_EVENT_CLICKED, overlay);

    // Título
    lv_obj_t* title = lv_label_create(overlay);
    lv_label_set_text(title, t(K_CONFIRMA_TU_PIN));
    lv_obj_set_style_text_color(title, _P_FG, 0);
    lv_obj_set_style_text_font(title, FONT_HDR, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    // Dots
    int dot_d = 14, dot_gap = 10;
    int dot_tot = PIN_DIGITS * dot_d + (PIN_DIGITS - 1) * dot_gap;
    int dot_x0  = (PIN_W - dot_tot) / 2;
    int dot_y   = 56;
    for (int i = 0; i < PIN_DIGITS; i++) {
        _pin_verify_dots[i] = lv_obj_create(overlay);
        lv_obj_set_size(_pin_verify_dots[i], dot_d, dot_d);
        lv_obj_set_pos(_pin_verify_dots[i], dot_x0 + i * (dot_d + dot_gap), dot_y);
        lv_obj_set_style_radius(_pin_verify_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(_pin_verify_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(_pin_verify_dots[i], 2, 0);
        lv_obj_set_style_pad_all(_pin_verify_dots[i], 0, 0);
    }
    _pin_verify_update_dots();

    // Status
    _pin_verify_status = lv_label_create(overlay);
    lv_label_set_text(_pin_verify_status, "");
    lv_obj_set_style_text_color(_pin_verify_status, _P_DANGER, 0);
    lv_obj_set_style_text_font(_pin_verify_status, FONT_SM, 0);
    lv_obj_align(_pin_verify_status, LV_ALIGN_TOP_MID, 0, dot_y + dot_d + 8);

    // Grid numérico
    int key_d = 43, gap = 8;
    int gw = 3 * key_d + 2 * gap;
    int gx = (PIN_W - gw) / 2;
    int gy = 88;

    const char* labels[] = {"1","2","3","4","5","6","7","8","9","","0","\xEF\x9C\x96"};
    for (int i = 0; i < 12; i++) {
        int col = i % 3, row = i / 3;
        if (i == 9) continue; // vacío
        lv_obj_t* btn = lv_btn_create(overlay);
        lv_obj_set_size(btn, key_d, key_d);
        lv_obj_set_pos(btn, gx + col * (key_d + gap), gy + row * (key_d + gap));
        lv_obj_set_style_bg_color(btn, _P_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_color(lbl, _P_FG, 0);
        lv_obj_set_style_text_font(lbl, FONT_HDR, 0);
        lv_obj_center(lbl);

        struct VKeyCtx { int idx; lv_obj_t* ov; };
        static VKeyCtx vkctx[12];
        vkctx[i] = {i, overlay};

        lv_obj_add_event_cb(btn, [](lv_event_t* e){
            vibro_pulse();
            VKeyCtx* ctx = (VKeyCtx*)lv_event_get_user_data(e);
            int idx = ctx->idx;
            bool is_del = (idx == 11);
            if (is_del) {
                if (_pin_verify_len > 0) { _pin_verify_len--; _pin_verify_buf[_pin_verify_len] = 0; }
            } else if (_pin_verify_len < PIN_DIGITS) {
                const char* digits[] = {"1","2","3","4","5","6","7","8","9","","0",""};
                _pin_verify_buf[_pin_verify_len++] = digits[idx][0];
            }
            _pin_verify_update_dots();
            if (_pin_verify_len == PIN_DIGITS) {
                if (vault_verify_pin(_pin_verify_buf)) {
                    lv_obj_t* ov = ctx->ov;
                    memset(_pin_verify_buf, 0, sizeof(_pin_verify_buf));
                    _pin_verify_len = 0;
                    lv_obj_del(ov);
                    lv_obj_clean(lv_layer_top());
                    if (_pin_verify_cb) _pin_verify_cb();
                } else {
                    if (_pin_verify_status && lv_obj_is_valid(_pin_verify_status))
                        lv_label_set_text(_pin_verify_status, t(K_PIN_INCORRECTO));
                    memset(_pin_verify_buf, 0, sizeof(_pin_verify_buf));
                    _pin_verify_len = 0;
                    _pin_verify_update_dots();
                }
            }
        }, LV_EVENT_CLICKED, &vkctx[i]);
    }
}
