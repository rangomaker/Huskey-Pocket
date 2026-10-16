#pragma once
// =============================================================
//  ui_statusbar.h — barra de estado (batería + hora)
//
//  Batería: fill continuo que salta en pasos de 25%, nub izquierda.
//  Cargando: el fill parpadea. Sin label de %.
// =============================================================
#include <lvgl.h>
#include "vault.h"
#include "colors.h"
#include "fonts.h"
#include "rtc_manager.h"
#include "battery_manager.h"

static lv_obj_t*   _sb_root       = nullptr;
static lv_obj_t*   _sb_clock_lbl  = nullptr;
static lv_obj_t*   _sb_batt_shell = nullptr;
static lv_obj_t*   _sb_batt_nub   = nullptr;
static lv_obj_t*   _sb_batt_fill  = nullptr;
static lv_obj_t*   _sb_batt_blink = nullptr; // segmento próximo a cargar (parpadea)
static lv_timer_t* _sb_blink_tmr  = nullptr;
static bool        _sb_blink_on   = true;
static bool        _sb_charging   = false;
static lv_color_t  _sb_seg_color  = {};
static lv_color_t  _sb_danger_col = {};
static uint32_t    _sb_last_update= 0;

#define SB_SHELL_W  22
#define SB_SHELL_H  11
// Interior usable: SB_SHELL_W-2(borde)-2(inset) = 18px ancho, SB_SHELL_H-2-2 = 7px alto
#define SB_FILL_MAX 18
#define SB_FILL_H    7

static void _sb_blink_cb(lv_timer_t*) {
    if (!_sb_charging || !_sb_batt_blink) return;
    _sb_blink_on = !_sb_blink_on;
    lv_obj_set_style_bg_opa(_sb_batt_blink, _sb_blink_on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

static void _statusbar_apply_theme(int t) {
    if (!_sb_root) return;
    int base = (t == THEME_PERSONALIZADO) ? vault_get_custom_theme().base : t;
    lv_color_t bg, fg, muted;
    if (base == 0) { bg = lv_color_hex(0x1C1C1E); fg = lv_color_hex(0xFFFFFF); muted = lv_color_hex(0x8E8E93); }
    else           { bg = lv_color_hex(0xE7EBF1); fg = lv_color_hex(0x000000); muted = lv_color_hex(0x999999); }
    if (t == THEME_PERSONALIZADO) {
        VaultCustomTheme ct = vault_get_custom_theme();
        bg    = hsv_to_lv_color(ct.bg_h, ct.bg_s, base == 0 ? 16 : 94);
        fg    = color_is_light(bg) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
        muted = lv_color_mix(fg, bg, 140);
    }
    _sb_seg_color  = fg;
    _sb_danger_col = lv_color_hex(0xFF3B30);

    lv_obj_set_style_bg_color(_sb_root, bg, 0);
    lv_obj_set_style_bg_opa(_sb_root, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(_sb_clock_lbl, fg, 0);
    lv_obj_set_style_border_color(_sb_batt_shell, muted, 0);
    lv_obj_set_style_bg_color(_sb_batt_nub, muted, 0);
    if (_sb_batt_fill) lv_obj_set_style_bg_color(_sb_batt_fill, fg, 0);
    if (_sb_batt_blink) lv_obj_set_style_bg_color(_sb_batt_blink, fg, 0);
    lv_obj_invalidate(_sb_root);
}

void ui_statusbar_apply_theme(int t) { _statusbar_apply_theme(t); }

void ui_statusbar_set_visible(bool visible) {
    if (!_sb_root) return;
    if (visible) lv_obj_clear_flag(_sb_root, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(_sb_root, LV_OBJ_FLAG_HIDDEN);
}

void ui_statusbar_init() {
    if (_sb_root) return;

    lv_obj_t* top = lv_layer_top();
    _sb_root = lv_obj_create(top);
    lv_obj_remove_style_all(_sb_root);
    lv_obj_set_size(_sb_root, UI_W, STATUSBAR_H);
    lv_obj_set_pos(_sb_root, 0, 0);
    lv_obj_clear_flag(_sb_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_sb_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_sb_root, LV_OBJ_FLAG_HIDDEN);

    _sb_clock_lbl = lv_label_create(_sb_root);
    lv_obj_set_style_text_font(_sb_clock_lbl, FONT_SM, 0);
    lv_label_set_text(_sb_clock_lbl, "--:--");
    lv_obj_align(_sb_clock_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    // ── Carcasa — nub a la izquierda ─────────────────────────
    _sb_batt_shell = lv_obj_create(_sb_root);
    lv_obj_remove_style_all(_sb_batt_shell);
    lv_obj_set_size(_sb_batt_shell, SB_SHELL_W, SB_SHELL_H);
    lv_obj_set_style_border_width(_sb_batt_shell, 1, 0);
    lv_obj_set_style_radius(_sb_batt_shell, 2, 0);
    lv_obj_set_style_bg_opa(_sb_batt_shell, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(_sb_batt_shell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(_sb_batt_shell, LV_ALIGN_RIGHT_MID, -6, 0);

    _sb_batt_nub = lv_obj_create(_sb_root);
    lv_obj_remove_style_all(_sb_batt_nub);
    lv_obj_set_size(_sb_batt_nub, 2, 5);
    lv_obj_set_style_radius(_sb_batt_nub, 1, 0);
    lv_obj_set_style_bg_opa(_sb_batt_nub, LV_OPA_COVER, 0);
    lv_obj_align_to(_sb_batt_nub, _sb_batt_shell, LV_ALIGN_OUT_LEFT_MID, 0, 0);

    // ── Fill continuo ─────────────────────────────────────────
    lv_obj_update_layout(_sb_root);
    lv_coord_t sx = lv_obj_get_x(_sb_batt_shell);
    lv_coord_t sy = lv_obj_get_y(_sb_batt_shell);

    _sb_batt_fill = lv_obj_create(_sb_root);
    lv_obj_remove_style_all(_sb_batt_fill);
    lv_obj_set_style_radius(_sb_batt_fill, 1, 0);
    lv_obj_set_style_bg_opa(_sb_batt_fill, LV_OPA_COVER, 0);
    lv_obj_set_size(_sb_batt_fill, 0, SB_FILL_H);
    // centrado vertical: (SB_SHELL_H - SB_FILL_H) / 2 = 2px
    lv_obj_set_pos(_sb_batt_fill, sx + 2, sy + (SB_SHELL_H - SB_FILL_H) / 2);
    lv_obj_clear_flag(_sb_batt_fill, LV_OBJ_FLAG_SCROLLABLE);

    _sb_batt_blink = lv_obj_create(_sb_root);
    lv_obj_remove_style_all(_sb_batt_blink);
    lv_obj_set_style_radius(_sb_batt_blink, 1, 0);
    lv_obj_set_style_bg_opa(_sb_batt_blink, LV_OPA_TRANSP, 0);
    lv_obj_set_size(_sb_batt_blink, SB_FILL_MAX / 4, SB_FILL_H);
    lv_obj_set_pos(_sb_batt_blink, sx + 2, sy + (SB_SHELL_H - SB_FILL_H) / 2);
    lv_obj_clear_flag(_sb_batt_blink, LV_OBJ_FLAG_SCROLLABLE);

    _sb_blink_tmr = lv_timer_create(_sb_blink_cb, 400, nullptr);
    _statusbar_apply_theme(vault_get_theme());
}

void ui_statusbar_tick(bool force) {
    if (!_sb_root) { ui_statusbar_init(); if (!_sb_root) return; }

    uint32_t now = millis();
    if (!force && now - _sb_last_update < 1000) return;
    _sb_last_update = now;

    char tbuf[8];
    rtc_get_time_str(tbuf, sizeof(tbuf));
    lv_label_set_text(_sb_clock_lbl, tbuf);

    int pct = 0; bool charging = false;
    battery_get_status(&pct, &charging);
    _sb_charging = charging;

    // Nivel redondeado al cuarto más cercano
    int level = (pct + 12) / 25;
    if (level > 4) level = 4;
    static const int fill_steps[5] = { 0,
        SB_FILL_MAX / 4,
        SB_FILL_MAX / 2,
        SB_FILL_MAX * 3 / 4,
        SB_FILL_MAX };
    int fill_w = fill_steps[level];

    lv_color_t col = (pct <= 15 && !charging) ? _sb_danger_col : _sb_seg_color;
    lv_obj_set_style_bg_color(_sb_batt_fill, col, 0);
    lv_obj_set_width(_sb_batt_fill, fill_w);
    // Anclar a la derecha del interior: x se recalcula cada tick
    lv_obj_set_x(_sb_batt_fill, lv_obj_get_x(_sb_batt_shell) + 2 + (SB_FILL_MAX - fill_w));

    // Fill: siempre sólido al nivel actual
    lv_obj_set_style_bg_opa(_sb_batt_fill, fill_w > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);

    // Segmento parpadeante: el SIGUIENTE a cargar (a la izquierda del fill)
    lv_color_t blink_col = (pct <= 15 && !charging) ? _sb_danger_col : _sb_seg_color;
    lv_obj_set_style_bg_color(_sb_batt_blink, blink_col, 0);
    if (charging && level < 4) {
        // Ancho del siguiente segmento = un cuarto del interior
        int seg_w = SB_FILL_MAX / 4;
        // X: justo a la izquierda del fill actual
        int blink_x = lv_obj_get_x(_sb_batt_shell) + 2 + (SB_FILL_MAX - fill_w) - seg_w;
        if (blink_x < lv_obj_get_x(_sb_batt_shell) + 2) blink_x = lv_obj_get_x(_sb_batt_shell) + 2;
        lv_obj_set_x(_sb_batt_blink, blink_x);
        lv_obj_set_width(_sb_batt_blink, seg_w);
        // opa la maneja _sb_blink_cb
    } else {
        lv_obj_set_style_bg_opa(_sb_batt_blink, LV_OPA_TRANSP, 0);
    }
}
