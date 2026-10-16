// =============================================================
//  password_manager.ino — Sketch principal
//  Waveshare ESP32-S3-Touch-LCD-2.8
//  Display: ST7789 driver propio Waveshare (Display_ST7789.h/cpp)
//  Touch: CST328 (Wire1) | RTC: PCF85063 (Wire)
// =============================================================

#define KEYVAULT_VERSION "1.0.1"
#define KEYVAULT_BUILD   "2026"

#include "ble_keyboard_manager.h"
#include "usb_keyboard_manager.h"

#include <lvgl.h>
#include "Display_ST7789.h"   // driver Waveshare — incluye Touch_CST328.h internamente

#include "vault.h"
#include "rtc_manager.h"
#include "battery_manager.h"
#include "colors.h"
#include "fonts.h"

#include <FS.h>
using namespace fs;

#include "sd_manager.h"
#include "vault_csv.h"
#include "vault_hky.h"
#include "custom_kb.h"
#include "custom_kb_t9.h"
#include "kb_manager.h"
#include "lock_screen.h"
#include "pin_screen.h"
#include "ui_main.h"
#include "power_ui.h"
#include "onboarding.h"
#include "usb_msc_manager.h"
#include "vibro_manager.h"
#include "power_manager.h"
#include "snatch_lock.h"
#include "i18n.h"
#include "screenshot.h"

SET_LOOP_TASK_STACK_SIZE(32 * 1024);

static const uint16_t SCREEN_W = 240;
static const uint16_t SCREEN_H = 320;
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf[SCREEN_W * 20];

// ── Escritura unificada USB/BLE ───────────────────────────────
void kb_type_string(const char* str) {
    if (vault_get_kb_mode() == 1) ble_type_string(str);
    else                          usb_type_string(str);
}
void kb_type_tab() {
    if (vault_get_kb_mode() == 1) ble_type_tab();
    else                          usb_type_tab();
}
void kb_type_enter() {
    if (vault_get_kb_mode() == 1) ble_type_enter();
    else                          usb_type_enter();
}
void kb_type_user(const char* u)   { kb_type_string(u); }
void kb_type_pass(const char* p)   { kb_type_string(p); }
void kb_type_secret(const char* s) { kb_type_string(s); }
void kb_type_totp(const char* name) {
    int code = vault_totp_now(name, nullptr);
    if (code < 0) return;
    char buf[8]; snprintf(buf, sizeof(buf), "%06d", code);
    kb_type_string(buf);
}

// ── Callback PIN correcto ─────────────────────────────────────
void on_unlock(bool success) {
    if (!success) return;
    Serial.println("[APP] Vault desbloqueado");
    power_set_locked(false);

    // El advertising BLE se inicia tras validar el PIN
    if (vault_get_kb_mode() == 1) {
        ble_kb_begin();
    }

    ui_main_show();
}

// ── LVGL flush — usa LCD_addWindow del driver Waveshare ───────
void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    LCD_addWindow(area->x1, area->y1, area->x2, area->y2, (uint16_t*)&color_p->full);
    lv_disp_flush_ready(disp);
}

// ── Gestos de swipe ───────────────────────────────────────────
static int16_t _swipe_x0     = -1;
static int16_t _swipe_y0     = -1;
static int16_t _swipe_x1     = -1;
static int16_t _swipe_y1     = -1;
static bool    _swipe_active = false;
static bool    _swipe_opened = false;
#define SWIPE_EDGE_MAX  20
#define SWIPE_MIN_DIST  50

void ui_main_swipe_right();
void ui_main_swipe_drag(int16_t dx);

// ── LVGL touch ────────────────────────────────────────────────
void my_touchpad_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data) {
    uint16_t x[1] = {0}, y[1] = {0}, strength[1] = {0};
    uint8_t  point_num = 0;

    if (Touch_interrupts) {
        Touch_interrupts = false;
        Touch_Read_Data();
    }

    bool pressed = Touch_Get_XY(x, y, strength, &point_num, 1);

    // Debounce: ignorar nuevo toque dentro de 100ms del último release
    static uint32_t _last_release_ms = 0;
    static bool     _was_pressed     = false;
    if (pressed && !_was_pressed) {
        if (millis() - _last_release_ms < 150) {
            data->state = LV_INDEV_STATE_REL;
            return;
        }
    }
    if (!pressed && _was_pressed) _last_release_ms = millis();
    _was_pressed = pressed;

    if (pressed && point_num > 0) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = x[0];
        data->point.y = y[0];
        pin_screen_activity();

        if (!_swipe_active && x[0] < SWIPE_EDGE_MAX && y[0] < 220) {
            _swipe_active = true;
            _swipe_opened = false;
            _swipe_x0     = x[0];
            _swipe_y0     = y[0];
            data->state = LV_INDEV_STATE_REL;
        }
        if (_swipe_active) {
            _swipe_x1 = x[0];
            _swipe_y1 = y[0];
            int16_t dx = _swipe_x1 - _swipe_x0;
            int16_t dy = abs(_swipe_y1 - _swipe_y0);
            if (dx > 0 && dy < dx) {
                ui_main_swipe_drag(dx);
                data->state = LV_INDEV_STATE_REL;
            }
        }
    } else {
        if (_swipe_active) {
            if (!_swipe_opened) ui_main_swipe_drag(-1);
            _swipe_active = false;
            _swipe_opened = false;
            _swipe_x0 = _swipe_y0 = _swipe_x1 = _swipe_y1 = -1;
        }
        data->state = LV_INDEV_STATE_REL;
    }
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    uint32_t _boot_t0 = millis();
    power_begin(); // sostiene la alimentación (GPIO7) — ANTES que cualquier otra cosa, incluso Serial

    Serial.begin(115200);
    Serial.println("[APP] Iniciando Huskey Pocket...");
    Serial.printf("[APP] reset_reason=%d (5=deep sleep wake por EXT0, 1=power-on, 12=software restart)\n", (int)esp_reset_reason());

    vibro_begin();
    i18n_begin();
    battery_init();

    // Display + Touch (Touch_Init() se llama dentro de LCD_Init())
    Serial.println("[APP] LCD init...");
    Backlight_Init();
    Set_Backlight(0); // apagado durante init para evitar glitch
    LCD_Init();
    Serial.printf("[APP] LCD init OK (%lu ms desde boot)\n", millis() - _boot_t0);

    // RTC (Wire, SDA=11, SCL=10)
    rtc_begin();
    Serial.printf("[APP] rtc_begin OK (%lu ms desde boot)\n", millis() - _boot_t0);

    // IMU QMI8658 (mismo bus Wire que el RTC) — usado por Snatch Lock
    snatch_lock_init();

    vault_deadman_check();

    // ── Modo USB MSC ──────────────────────────────────────────
    if (vault_get_usb_msc_mode()) {
        Serial.println("[APP] Modo USB activo");

        lv_init();
        lv_disp_draw_buf_init(&disp_buf, buf, NULL, SCREEN_W * 20);
        static lv_disp_drv_t disp_drv;
        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res  = SCREEN_W;
        disp_drv.ver_res  = SCREEN_H;
        disp_drv.flush_cb = my_disp_flush;
        disp_drv.draw_buf = &disp_buf;
        lv_disp_drv_register(&disp_drv);

        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type    = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = my_touchpad_read;
        lv_indev_drv_register(&indev_drv);

        usb_msc_begin();
        Serial.println("[APP] Modo USB listo");
        return;
    }

    if (vault_get_kb_mode() == 1) {
        Serial.println("[APP] Modo teclado: Bluetooth (se activa al desbloquear)");
        // ble_kb_begin() se inicializa en on_unlock() para evitar advertising mientras esté bloqueado.
        // USB CDC on Boot es gestionado automáticamente por el core del ESP32-S3.
    } else {
        Serial.println("[APP] Modo teclado: USB");
        usb_kb_begin();
    }

    // ── LVGL ─────────────────────────────────────────────────
    lv_init();
    Serial.println("[APP] lv_init OK");

    lv_disp_draw_buf_init(&disp_buf, buf, NULL, SCREEN_W * 20);
    Serial.println("[APP] draw_buf OK");

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_W;
    disp_drv.ver_res  = SCREEN_H;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);
    Serial.println("[APP] disp_drv OK");

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    Serial.println("[APP] indev OK");

    pin_screen_init(on_unlock);
    Serial.printf("[APP] pin_screen_init OK (%lu ms desde boot — UI lista)\n", millis() - _boot_t0);

    Serial.printf("[APP] Tema guardado: %d\n", vault_get_theme_raw());
    if (!vault_onboarding_done()) {
        onboarding_show();
    } else {
        lock_screen_show();
    }
    lv_task_handler();
    lv_task_handler();
    lv_task_handler();
    delay(100);
    Set_Backlight(100);

    Serial.println("[APP] Listo");
    // El montaje y la limpieza de la microSD no bloquean la pantalla de acceso.
    // /import se conserva; la tarjeta no se publica hasta terminar la limpieza.
    sd_begin_startup_async();
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
    power_tick(); // primero — detecta hold forzado (10s) aunque LVGL esté trabado
    lv_task_handler();
    pin_screen_tick();
    snatch_lock_tick();
    ui_statusbar_tick();
    ble_kb_tick(); // procesa pedidos de la tarea BLE (popup de emparejamiento)
    screenshot_poll(); // comando "shot" por serial → BMP en /export
    delay(5);
}
