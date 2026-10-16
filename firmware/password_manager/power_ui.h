#pragma once
// =============================================================
//  power_ui.h — Menú Apagar / Reiniciar / Cancelar
// =============================================================
#include "power_manager.h"

void lock_screen_show(); // forward decl — definida en lock_screen.h

void power_show_menu(bool show_lock = true) {    lv_obj_t* scr_act = lv_scr_act();
    if (!scr_act) return;

    // ── Colores propios (no depende de globales C_* no inicializados) ──
    int theme = vault_get_theme();
    int base  = (theme == THEME_PERSONALIZADO) ? vault_get_custom_theme().base : theme;
    lv_color_t bg, card, fg;
    if (base == 0) {
        bg   = lv_color_hex(0x1C1C1E);
        card = lv_color_hex(0x2C2C2E);
        fg   = lv_color_hex(0xFFFFFF);
    } else {
        bg   = lv_color_hex(0xE7EBF1);
        card = lv_color_hex(0xFFFFFF);
        fg   = lv_color_hex(0x000000);
    }
    if (theme == THEME_PERSONALIZADO) {
        VaultCustomTheme ct = vault_get_custom_theme();
        bg   = hsv_to_lv_color(ct.bg_h, ct.bg_s, base == 0 ? 16 : 94);
        card = hsv_to_lv_color(ct.card_h, ct.card_s, base == 0 ? 24 : 100);
        fg   = color_is_light(bg) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
    }
    int ai = vault_get_accent();
    lv_color_t accent;
    if (theme == THEME_PERSONALIZADO) {
        VaultCustomTheme ct = vault_get_custom_theme();
        accent = hsv_to_lv_color(ct.acc_h, ct.acc_s, base == 0 ? 80 : 70);
    } else if (ai >= 0 && ai < ACCENT_COUNT) {
        accent = lv_color_hex(ACCENT_PALETTE[ai]);
    } else {
        accent = base == 0 ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
    }
    lv_color_t white = lv_color_hex(0xFFFFFF);
    lv_color_t ico_color = color_is_light(accent) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);

    // ── Overlay oscuro ────────────────────────────────────────
    lv_obj_t* overlay = lv_obj_create(scr_act);
    lv_obj_set_size(overlay, UI_W, UI_H);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    _fade_overlay(overlay, LV_OPA_50);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(overlay, [](lv_event_t* ev){
        lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
    }, LV_EVENT_CLICKED, overlay);

    // ── Dimensiones del popup ────────────────────────────────
    const int CIRC  = 58;  // diámetro de cada círculo
    const int LABEL = 18;  // altura texto bajo el círculo
    const int PAD   = 20;  // padding interno
    const int GAP   = 14;  // separación entre círculos

    int cols   = 3;
    int rows   = 1;
    int pop_w  = PAD + cols * CIRC + (cols - 1) * GAP + PAD;
    int cell_h = CIRC + 4 + LABEL;
    int pop_h  = PAD + rows * cell_h + (rows - 1) * GAP + PAD;
    int pop_x  = (UI_W - pop_w) / 2;
    int pop_y  = (UI_H - pop_h) / 2;

    // ── Popup flotante ────────────────────────────────────────
    lv_obj_t* popup = lv_obj_create(overlay);
    lv_obj_set_size(popup, pop_w, pop_h);
    lv_obj_set_pos(popup, pop_x, pop_y);
    lv_obj_set_style_bg_color(popup, bg, 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(popup, 0, 0);
    lv_obj_set_style_radius(popup, 20, 0);
    lv_obj_set_style_pad_all(popup, 0, 0);
    lv_obj_set_scrollbar_mode(popup, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(popup, [](lv_event_t* ev){
        lv_event_stop_bubbling(ev);
    }, LV_EVENT_CLICKED, nullptr);

    // ── Helper: botón circular ───────────────────────────────
    auto circle_btn = [&](int col, int row, const char* icon,
                           const char* label, lv_event_cb_t cb, void* ud) {
        int x = PAD + col * (CIRC + GAP);
        int y = PAD + row * (cell_h + GAP);

        lv_obj_t* circ = lv_obj_create(popup);
        lv_obj_set_size(circ, CIRC, CIRC);
        lv_obj_set_pos(circ, x, y);
        lv_obj_set_style_radius(circ, CIRC / 2, 0);
        lv_obj_set_style_bg_color(circ, accent, 0);
        lv_obj_set_style_bg_opa(circ, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(circ, 0, 0);
        lv_obj_set_style_pad_all(circ, 0, 0);
        lv_obj_clear_flag(circ, LV_OBJ_FLAG_SCROLLABLE);
        _press_fb(circ);
        if (cb) lv_obj_add_event_cb(circ, cb, LV_EVENT_CLICKED, ud);

        lv_obj_t* ico = lv_label_create(circ);
        lv_label_set_text(ico, icon);
        lv_obj_set_style_text_font(ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico, ico_color, 0);
        lv_obj_center(ico);
        lv_obj_clear_flag(ico, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(popup);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_font(lbl, FONT_SM, 0);
        lv_obj_set_style_text_color(lbl, fg, 0);
        lv_obj_set_width(lbl, CIRC + GAP);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lbl, x - GAP / 2, y + CIRC + 4);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    };

    // ── Botones: Apagar | Reiniciar | Cancelar ───────────────
    circle_btn(0, 0, ICO_WARNING,   t(K_APAGAR),    [](lv_event_t* e){ power_shutdown(); }, nullptr);
    circle_btn(1, 0, ICO_AUTORENEW, t(K_REINICIAR), [](lv_event_t* e){ power_restart();  }, nullptr);
    circle_btn(2, 0, ICO_CLOSE,     t(K_CANCELAR),  [](lv_event_t* ev){
        lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
    }, overlay);

    // ── Animación: slide up desde ligeramente abajo ──────────
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, popup);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, pop_y + 24, pop_y);
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}
