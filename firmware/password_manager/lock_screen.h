#pragma once
// =============================================================
//  lock_screen.h — Pantalla de "deslizar para desbloquear", se
//  muestra ANTES del PIN tanto al arrancar como al auto-bloquearse.
//  Reloj grande + fecha + batería, con un gesto real de arrastre
//  (no solo un botón) — si el dedo se sale del área de la barra
//  mientras sigue presionado, el círculo se cancela y regresa solo.
//
//  Igual que pin_screen.h, es autónoma de los colores de ui_main.h
//  (corre antes de que la bóveda esté lista, así que no depende de
//  _apply_theme() haber corrido primero).
// =============================================================
#include <lvgl.h>
#include "vault.h"
#include "colors.h"
#include "vibro_manager.h"
#include "rtc_manager.h"
#include "battery_manager.h"
#include "fonts.h"
#include "i18n.h"

void lock_screen_show();
void pin_screen_show(); // definida en pin_screen.h (incluido después)
void ui_statusbar_set_visible(bool visible); // definida en ui_main.h
void power_set_locked(bool locked);          // definida en power_manager.h

#define LOCK_W 240
#define LOCK_H 320

static lv_obj_t* _lock_time_lbl = nullptr;
static lv_obj_t* _lock_date_lbl = nullptr;
static lv_obj_t* _lock_batt_shell = nullptr;
static lv_obj_t* _lock_batt_nub   = nullptr;
static lv_obj_t* _lock_batt_fill  = nullptr;
static lv_obj_t* _lock_batt_blink = nullptr;
static bool      _lock_batt_blink_on = false;
static lv_timer_t* _lock_blink_tmr = nullptr;
#define LOCK_SHELL_W  22
#define LOCK_SHELL_H  11
#define LOCK_FILL_MAX 18
#define LOCK_FILL_H    7
static lv_obj_t* _lock_track    = nullptr;
static lv_obj_t* _lock_handle   = nullptr;
static lv_timer_t* _lock_tick_timer = nullptr;

#define LOCK_HANDLE_W 26
#define LOCK_HANDLE_PAD 3

static void _lock_snap_back() {
    if (!_lock_handle) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _lock_handle);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, lv_obj_get_x(_lock_handle), LOCK_HANDLE_PAD);
    lv_anim_set_time(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void _lock_unlock_now() {
    if (_lock_tick_timer) { lv_timer_del(_lock_tick_timer); _lock_tick_timer = nullptr; }
    if (_lock_blink_tmr) { lv_timer_del(_lock_blink_tmr); _lock_blink_tmr = nullptr; }
    lv_async_call([](void*){ pin_screen_show(); }, nullptr);
}

static bool _lock_drag_valid = false; // el gesto empezó sobre el círculo

static void _lock_track_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (!_lock_track || !_lock_handle) return;

    lv_area_t track_area;
    lv_obj_get_coords(_lock_track, &track_area);
    int max_x = (track_area.x2 - track_area.x1) - LOCK_HANDLE_W - LOCK_HANDLE_PAD;

    if (code == LV_EVENT_PRESSED) {
        // Validar que el gesto inicie sobre el círculo deslizante (margen ±8px)
        lv_indev_t* indev = lv_indev_get_act();
        lv_point_t p; lv_indev_get_point(indev, &p);
        lv_area_t ha; lv_obj_get_coords(_lock_handle, &ha);
        _lock_drag_valid = (p.x >= ha.x1 - 8 && p.x <= ha.x2 + 8 &&
                            p.y >= ha.y1 - 8 && p.y <= ha.y2 + 8);
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (!_lock_drag_valid) return;
        lv_indev_t* indev = lv_indev_get_act();
        lv_point_t p; lv_indev_get_point(indev, &p);

        // Si el toque se sale verticalmente del área de la barra
        // (con un margen razonable), se cancela el gesto.
        if (p.y < track_area.y1 - 24 || p.y > track_area.y2 + 24) {
            vibro_off();
            _lock_snap_back();
            return;
        }

        vibro_on(); // sigue encendido mientras el dedo se mantenga arrastrando

        int local_x = p.x - track_area.x1 - (LOCK_HANDLE_W / 2);
        if (local_x < LOCK_HANDLE_PAD) local_x = LOCK_HANDLE_PAD;
        if (local_x > max_x) local_x = max_x;
        lv_obj_set_x(_lock_handle, local_x);

    } else if (code == LV_EVENT_RELEASED) {
        vibro_off();
        bool was_valid = _lock_drag_valid;
        _lock_drag_valid = false;
        if (!was_valid) return; // toque que no empezó en el círculo: ignorar
        int x = lv_obj_get_x(_lock_handle);
        if (x >= max_x - 6) _lock_unlock_now();
        else                _lock_snap_back();

    } else if (code == LV_EVENT_PRESS_LOST) {
        vibro_off();
        _lock_drag_valid = false;
        _lock_snap_back();
    }
}

static void _lock_refresh(lv_timer_t* tmr) {
    if (_lock_time_lbl) {
        char buf[8];
        rtc_get_time_str(buf, sizeof(buf));
        lv_label_set_text(_lock_time_lbl, buf);
    }
    if (_lock_date_lbl) {
        const char* dias[7] = { t(K_DIA_0), t(K_DIA_1), t(K_DIA_2), t(K_DIA_3), t(K_DIA_4), t(K_DIA_5), t(K_DIA_6) };
        const char* meses[12] = { t(K_MES_0), t(K_MES_1), t(K_MES_2), t(K_MES_3), t(K_MES_4), t(K_MES_5),
                                   t(K_MES_6), t(K_MES_7), t(K_MES_8), t(K_MES_9), t(K_MES_10), t(K_MES_11) };
        char buf[40];
        int wd = rtc_get_weekday();
        if (wd < 0 || wd > 6) wd = 0;
        int mon = rtc_get_month() - 1;
        if (i18n_get_lang() == LANG_ES)
            snprintf(buf, sizeof(buf), "%s, %d de %s", dias[wd], rtc_get_day(), meses[mon]);
        else // en inglés el orden es Mes Dia, no Dia de Mes
            snprintf(buf, sizeof(buf), "%s, %s %d", dias[wd], meses[mon], rtc_get_day());
        lv_label_set_text(_lock_date_lbl, buf);
    }
    {
        int pct = 0; bool charging = false;
        battery_get_status(&pct, &charging);
        int level = (pct + 12) / 25; if (level > 4) level = 4;
        static const int fs[5] = {0, LOCK_FILL_MAX/4, LOCK_FILL_MAX/2, LOCK_FILL_MAX*3/4, LOCK_FILL_MAX};
        int fw = fs[level];
        if (_lock_batt_fill) {
            lv_obj_set_width(_lock_batt_fill, fw);
            lv_obj_set_x(_lock_batt_fill, lv_obj_get_x(_lock_batt_shell) + 2 + (LOCK_FILL_MAX - fw));
            lv_obj_set_style_bg_opa(_lock_batt_fill, fw > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        }
        _lock_batt_blink_on = charging && level < 4;
        if (_lock_batt_blink && !charging)
            lv_obj_set_style_bg_opa(_lock_batt_blink, LV_OPA_TRANSP, 0);
        if (_lock_batt_blink && charging && level < 4) {
            static const int bfs[5] = {0, LOCK_FILL_MAX/4, LOCK_FILL_MAX/2, LOCK_FILL_MAX*3/4, LOCK_FILL_MAX};
            int bw = bfs[1]; // un segmento
            lv_obj_set_width(_lock_batt_blink, bw);
            int bx = lv_obj_get_x(_lock_batt_shell) + 2 + (LOCK_FILL_MAX - fw) - bw;
            if (bx < lv_obj_get_x(_lock_batt_shell) + 2) bx = lv_obj_get_x(_lock_batt_shell) + 2;
            lv_obj_set_x(_lock_batt_blink, bx);
        }
    }
}

void ble_kb_end(); // definida en ble_keyboard_manager.h

void lock_screen_show() {
    power_set_locked(true);
    ble_kb_end();
    int theme = vault_get_theme();
    lv_color_t bg, fg, muted, card, accent;
    int base = (theme == THEME_PERSONALIZADO) ? vault_get_custom_theme().base : theme;
    lv_color_t ok_bg_alt = (base == 0) ? lv_color_hex(0xE7EBF1) : lv_color_hex(0x1C1C1E);
    if (base == 0) { bg = lv_color_hex(0x1C1C1E); fg = lv_color_hex(0xFFFFFF); muted = lv_color_hex(0x8E8E93); card = lv_color_hex(0x2C2C2E); }
    else        { bg = lv_color_hex(0xE7EBF1); fg = lv_color_hex(0x000000); muted = lv_color_hex(0x666666); card = lv_color_hex(0xFFFFFF); }

    // Color de acento
    if (theme == THEME_PERSONALIZADO) {
        VaultCustomTheme ct = vault_get_custom_theme();
        accent = hsv_to_lv_color(ct.acc_h, ct.acc_s, 92);
    } else {
        int acc = vault_get_accent();
        if (acc == ACCENT_AUTO) accent = ok_bg_alt; // contraste invertido, igual patrón que pin_screen.h
        else {
            if (acc < 0 || acc >= ACCENT_COUNT) acc = 0;
            accent = lv_color_hex(ACCENT_PALETTE[acc]);
        }
    }
    if (theme == THEME_PERSONALIZADO) {
        VaultCustomTheme ct = vault_get_custom_theme();
        bg = hsv_to_lv_color(ct.bg_h, ct.bg_s, base == 0 ? 16 : 94);
        card = hsv_to_lv_color(ct.card_h, ct.card_s, base == 0 ? 24 : 100);
        fg = color_is_light(bg) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
        muted = lv_color_mix(fg, bg, 140); // gris intermedio entre texto y fondo
    }

    ui_statusbar_set_visible(false); // esta pantalla ya muestra su propia hora/batería

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Batería segmentada — esquina superior derecha
    _lock_batt_shell = lv_obj_create(scr);
    lv_obj_remove_style_all(_lock_batt_shell);
    lv_obj_set_size(_lock_batt_shell, LOCK_SHELL_W, LOCK_SHELL_H);
    lv_obj_set_style_border_width(_lock_batt_shell, 1, 0);
    lv_obj_set_style_border_color(_lock_batt_shell, muted, 0);
    lv_obj_set_style_radius(_lock_batt_shell, 2, 0);
    lv_obj_set_style_bg_opa(_lock_batt_shell, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(_lock_batt_shell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(_lock_batt_shell, LV_ALIGN_TOP_RIGHT, -8, 6);

    _lock_batt_nub = lv_obj_create(scr);
    lv_obj_remove_style_all(_lock_batt_nub);
    lv_obj_set_size(_lock_batt_nub, 2, 5);
    lv_obj_set_style_radius(_lock_batt_nub, 1, 0);
    lv_obj_set_style_bg_color(_lock_batt_nub, muted, 0);
    lv_obj_set_style_bg_opa(_lock_batt_nub, LV_OPA_COVER, 0);
    lv_obj_align_to(_lock_batt_nub, _lock_batt_shell, LV_ALIGN_OUT_LEFT_MID, 0, 0);

    lv_obj_update_layout(scr);
    lv_coord_t bsx = lv_obj_get_x(_lock_batt_shell);
    lv_coord_t bsy = lv_obj_get_y(_lock_batt_shell);

    _lock_batt_fill = lv_obj_create(scr);
    lv_obj_remove_style_all(_lock_batt_fill);
    lv_obj_set_style_radius(_lock_batt_fill, 1, 0);
    lv_obj_set_style_bg_color(_lock_batt_fill, fg, 0);
    lv_obj_set_style_bg_opa(_lock_batt_fill, LV_OPA_TRANSP, 0);
    lv_obj_set_size(_lock_batt_fill, 0, LOCK_FILL_H);
    lv_obj_set_pos(_lock_batt_fill, bsx + 2, bsy + (LOCK_SHELL_H - LOCK_FILL_H) / 2);
    lv_obj_clear_flag(_lock_batt_fill, LV_OBJ_FLAG_SCROLLABLE);

    _lock_batt_blink = lv_obj_create(scr);
    lv_obj_remove_style_all(_lock_batt_blink);
    lv_obj_set_style_radius(_lock_batt_blink, 1, 0);
    lv_obj_set_style_bg_color(_lock_batt_blink, fg, 0);
    lv_obj_set_style_bg_opa(_lock_batt_blink, LV_OPA_TRANSP, 0);
    lv_obj_set_size(_lock_batt_blink, LOCK_FILL_MAX / 4, LOCK_FILL_H);
    lv_obj_set_pos(_lock_batt_blink, bsx + 2, bsy + (LOCK_SHELL_H - LOCK_FILL_H) / 2);
    lv_obj_clear_flag(_lock_batt_blink, LV_OBJ_FLAG_SCROLLABLE);

    if (_lock_blink_tmr) { lv_timer_del(_lock_blink_tmr); _lock_blink_tmr = nullptr; }
    _lock_blink_tmr = lv_timer_create([](lv_timer_t*){
        if (!_lock_batt_blink_on || !_lock_batt_blink) return;
        static bool on = false; on = !on;
        lv_obj_set_style_bg_opa(_lock_batt_blink, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }, 400, nullptr);

    // Candado
    lv_obj_t* lock_ico = lv_label_create(scr);
    lv_label_set_text(lock_ico, ICO_LOCK);
    lv_obj_set_style_text_font(lock_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(lock_ico, muted, 0);
    lv_obj_align(lock_ico, LV_ALIGN_TOP_MID, 0, 46);

    // Hora grande
    _lock_time_lbl = lv_label_create(scr);
    lv_label_set_text(_lock_time_lbl, "--:--");
    lv_obj_set_style_text_color(_lock_time_lbl, fg, 0);
    lv_obj_set_style_text_font(_lock_time_lbl, FONT_HDR_LG, 0);
    lv_obj_align(_lock_time_lbl, LV_ALIGN_TOP_MID, 0, 72);

    // Fecha chica debajo
    _lock_date_lbl = lv_label_create(scr);
    lv_label_set_text(_lock_date_lbl, "");
    lv_obj_set_style_text_color(_lock_date_lbl, muted, 0);
    lv_obj_set_style_text_font(_lock_date_lbl, FONT_SM, 0);
    lv_obj_align(_lock_date_lbl, LV_ALIGN_TOP_MID, 0, 118);

    // Mensaje arriba de la barra
    lv_obj_t* msg = lv_label_create(scr);
    lv_label_set_text(msg, t(K_DESLIZA_DESBLOQUEAR));
    lv_obj_set_style_text_color(msg, muted, 0);
    lv_obj_set_style_text_font(msg, FONT_SM, 0);
    lv_obj_align(msg, LV_ALIGN_BOTTOM_MID, 0, -56);

    // Barra estilo iOS con el círculo deslizable
    lv_obj_t* track = lv_obj_create(scr);
    _lock_track = track;
    lv_obj_set_size(track, LOCK_W - 28, 32);
    lv_obj_align(track, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_bg_color(track, card, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_radius(track, 16, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(track, _lock_track_event_cb, LV_EVENT_ALL, nullptr);

    lv_obj_t* handle = lv_obj_create(track);
    _lock_handle = handle;
    lv_obj_remove_style_all(handle);
    lv_obj_set_size(handle, LOCK_HANDLE_W, LOCK_HANDLE_W);
    lv_obj_set_pos(handle, LOCK_HANDLE_PAD, (32 - LOCK_HANDLE_W) / 2);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(handle, accent, 0);
    lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, 0);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* arrow = lv_label_create(handle);
    lv_label_set_text(arrow, ICO_CHEVRON_RIGHT);
    lv_obj_set_style_text_font(arrow, FONT_ICON, 0);
    lv_obj_set_style_text_color(arrow, color_is_light(accent) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(arrow);
    lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE);

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);

    _lock_refresh(nullptr);
    if (_lock_tick_timer) lv_timer_del(_lock_tick_timer);
    _lock_tick_timer = lv_timer_create(_lock_refresh, 1000, nullptr);
}
