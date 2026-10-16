#pragma once
// =============================================================
//  ui_settings.h — Ajustes, Info/Privacidad/Acerca de, Cambiar PIN,
//  PIN señuelo (info paginada + setup). Extraído de ui_main.h.
//  Depende de helpers/colores/fuentes definidos antes en ui_main.h.
// =============================================================
#include <lvgl.h>
#include "vault.h"
#include "i18n.h"
#include "ble_keyboard_manager.h" // NimBLEDevice, NimBLEAddress (getNumBonds/deleteBond)

// ── PANTALLA: Ajustes ─────────────────────────────────────────
static void _show_date_settings();
static void _show_time_settings();
static void _show_deadman_info();
static void _show_deadman_settings_sheet();
static void _show_snatch_info();
static void _show_snatch_settings_sheet();

// Muestra advertencia al cambiar modo de teclado (USB/BLE) si no fue omitida.
static void _show_kb_mode_warning(int next) {
    // Si ya se mostró el popup y el usuario marcó "no volver a mostrar"
    if (vault_get_kb_popup_shown()) {
        vault_set_kb_mode(next);
        delay(300); ESP.restart();
        return;
    }

    // Popup modo teclado — layout posicional fijo (sin flex ni LV_SIZE_CONTENT)
    lv_obj_t* scr_a = lv_scr_act();
    lv_obj_t* overlay = lv_obj_create(scr_a);
    lv_obj_set_size(overlay, UI_W, UI_H);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(overlay, [](lv_event_t* ev){
        lv_event_stop_bubbling(ev);
    }, LV_EVENT_CLICKED, nullptr);

    const int CW = UI_W - 32;
    const int CH = 260;
    const int CX = 16;
    const int CY = (UI_H - CH) / 2;

    lv_obj_t* card = lv_obj_create(overlay);
    lv_obj_set_size(card, CW, CH);
    lv_obj_set_pos(card, CX, CY);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, t(K_MODO_TECLADO));
    lv_obj_set_style_text_font(title, FONT_HDR, 0);
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_pos(title, 16, 14);

    lv_obj_t* desc = lv_label_create(card);
    lv_label_set_text(desc, t(K_KB_MODE_DESC));
    lv_obj_set_size(desc, CW - 32, 90);
    lv_obj_set_pos(desc, 16, 42);
    lv_obj_set_style_text_font(desc, FONT_SM, 0);
    lv_obj_set_style_text_color(desc, C_MUTED, 0);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);

    // Checkbox
    static bool _kb_dont_show = false;
    _kb_dont_show = false;

    lv_obj_t* chk_box = lv_obj_create(card);
    lv_obj_set_size(chk_box, 18, 18);
    lv_obj_set_pos(chk_box, 16, 148);
    lv_obj_set_style_radius(chk_box, 4, 0);
    lv_obj_set_style_bg_color(chk_box, C_BG, 0);
    lv_obj_set_style_bg_opa(chk_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chk_box, C_MUTED, 0);
    lv_obj_set_style_border_width(chk_box, 1, 0);
    lv_obj_set_style_pad_all(chk_box, 0, 0);
    lv_obj_clear_flag(chk_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chk_box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* chk_ico = lv_label_create(chk_box);
    lv_label_set_text(chk_ico, ICO_CHECK);
    lv_obj_set_style_text_font(chk_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(chk_ico, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(chk_ico);
    lv_obj_add_flag(chk_ico, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* chk_lbl = lv_label_create(card);
    lv_label_set_text(chk_lbl, t(K_NO_VOLVER_MOSTRAR));
    lv_obj_set_style_text_font(chk_lbl, FONT_SM, 0);
    lv_obj_set_style_text_color(chk_lbl, C_MUTED, 0);
    lv_obj_set_pos(chk_lbl, 42, 151);

    lv_obj_add_event_cb(chk_box, [](lv_event_t* ev){
        lv_obj_t* b = (lv_obj_t*)lv_event_get_target(ev);
        lv_obj_t* i = lv_obj_get_child(b, 0);
        _kb_dont_show = !_kb_dont_show;
        lv_obj_set_style_bg_color(b, _kb_dont_show ? C_ACCENT : C_BG, 0);
        lv_obj_set_style_border_color(b, _kb_dont_show ? C_ACCENT : C_MUTED, 0);
        if (_kb_dont_show) lv_obj_clear_flag(i, LV_OBJ_FLAG_HIDDEN);
        else               lv_obj_add_flag(i, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, nullptr);

    // Divisor
    lv_obj_t* div = lv_obj_create(card);
    lv_obj_set_size(div, CW, 1);
    lv_obj_set_pos(div, 0, 178);
    lv_obj_set_style_bg_color(div, C_BORDER, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);

    int BW = (CW - 40) / 2;
    // Cancelar
    lv_obj_t* btn_cancel = lv_obj_create(card);
    lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn_cancel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_cancel, BW, 40);
    lv_obj_set_pos(btn_cancel, 12, 184);
    lv_obj_set_style_bg_color(btn_cancel, C_BG, 0);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 10, 0);
    lv_obj_set_style_pad_all(btn_cancel, 0, 0);
    lv_obj_t* cancel_lbl = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_lbl, t(K_CANCELAR));
    lv_obj_set_style_text_font(cancel_lbl, FONT_MD, 0);
    lv_obj_set_style_text_color(cancel_lbl, C_MUTED, 0);
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* ev){
        lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
    }, LV_EVENT_CLICKED, nullptr);

    // OK
    static int _kb_next_mode = 0;
    _kb_next_mode = next;

    lv_obj_t* btn_ok = lv_obj_create(card);
    lv_obj_add_flag(btn_ok, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn_ok, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_ok, BW, 40);
    lv_obj_set_pos(btn_ok, 20 + BW, 184);
    lv_obj_set_style_bg_color(btn_ok, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn_ok, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 10, 0);
    lv_obj_set_style_pad_all(btn_ok, 0, 0);
    lv_obj_t* ok_lbl = lv_label_create(btn_ok);
    lv_label_set_text(ok_lbl, t(K_ENTENDIDO));
    lv_obj_set_style_text_font(ok_lbl, FONT_MD, 0);
    lv_obj_set_style_text_color(ok_lbl, C_ACCENT_TEXT, 0);
    lv_obj_center(ok_lbl);
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* ev){
        if (_kb_dont_show) vault_set_kb_popup_shown(true);
        vault_set_kb_mode(_kb_next_mode);
        lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
        delay(300);
        ESP.restart();
    }, LV_EVENT_CLICKED, nullptr);
}


static lv_obj_t* _settings_body = nullptr;   // contenedor scrollable de ajustes
static int32_t _settings_scroll_y = 0;       // posición a restaurar al volver de un sub-ajuste
static bool _settings_restore = false;       // true = restaurar en el próximo _show_settings

// Llamar ANTES de abrir un sub-ajuste para recordar dónde estaba el scroll.
static void _settings_remember_scroll() {
    if (_settings_body && lv_obj_is_valid(_settings_body)) {
        _settings_scroll_y = lv_obj_get_scroll_y(_settings_body);
        _settings_restore = true;
    }
}

static void _show_settings(bool animated) {
    kb_reset();
    _current_screen = SCREEN_SETTINGS; _kb_active = false;    lv_obj_t* scr = _make_screen();

    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_back = lv_btn_create(hdr);
    _press_fb(btn_back);
    lv_obj_set_size(btn_back, 40, 44);
    lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back);
    lv_label_set_text(back_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(back_ico, C_TEXT, 0);
    lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_center(back_ico);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){ kb_reset(); _settings_restore = false; _show_list(); },
                        LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, t(K_AJUSTES));
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    _divider(scr, 44);

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_W, UI_H - 44);
    lv_obj_set_pos(body, 0, 44);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    _settings_body = body;

    const int ROW_H = 50;
    const int CARD_X = 12;
    const int CARD_W = UI_W - 24;
    int y = 12;
    int px = 14, tx = 40, vx = CARD_W - 74;

    lv_obj_t* card = nullptr;
    int card_rows = 0;

    auto section_header = [&](const char* text){
        lv_obj_t* h = lv_label_create(body);
        lv_label_set_text(h, text);
        lv_obj_set_style_text_color(h, C_MUTED, 0);
        lv_obj_set_style_text_font(h, FONT_SM, 0);
        lv_obj_set_pos(h, 16, y);
        y += 24;
    };

    auto card_begin = [&](){
        card = lv_obj_create(body);
        lv_obj_set_size(card, CARD_W, ROW_H);
        lv_obj_set_pos(card, CARD_X, y);
        lv_obj_set_style_bg_color(card, C_CARD, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
        card_rows = 0;
    };

    auto card_end = [&](){
        y += 18;
    };

    auto setting_row = [&](const char* icon_sym, const char* label,
                            const char* value, bool danger, lv_event_cb_t cb) {
        if (card_rows > 0) {
            lv_obj_t* div = lv_obj_create(body);
            lv_obj_set_size(div, CARD_W, 2);
            lv_obj_set_pos(div, CARD_X, y);
            lv_obj_set_style_bg_color(div, C_BG, 0);
            lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(div, 0, 0);
            lv_obj_set_style_pad_all(div, 0, 0);
            lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
        }

        lv_obj_t* row = lv_obj_create(body);
        lv_obj_set_size(row, CARD_W, ROW_H);
        lv_obj_set_pos(row, CARD_X, y);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* ico = lv_label_create(row);
        lv_label_set_text(ico, icon_sym);
        lv_obj_set_style_text_color(ico, danger ? C_DANGER : C_TEXT, 0);
        lv_obj_set_style_text_font(ico, FONT_ICON, 0);
        lv_obj_set_pos(ico, px, (ROW_H - 16) / 2);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, danger ? C_DANGER : C_TEXT, 0);
        lv_obj_set_style_text_font(lbl, FONT_MD, 0);
        lv_obj_set_pos(lbl, tx, (ROW_H - 16) / 2);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, CARD_W - tx - 28);

        lv_obj_t* arr = lv_label_create(row);
        lv_label_set_text(arr, ICO_CHEVRON_RIGHT); lv_obj_set_style_text_font(arr, FONT_ICON, 0);
        lv_obj_set_style_text_color(arr, danger ? C_DANGER : C_MUTED, 0);
        lv_obj_set_style_text_font(arr, FONT_ICON, 0);
        lv_obj_set_pos(arr, CARD_W - 20, (ROW_H - 14) / 2);

        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

        if (cb) {
            lv_obj_t* btn_area = lv_obj_create(row);
            lv_obj_set_size(btn_area, CARD_W, ROW_H);
            lv_obj_set_pos(btn_area, 0, 0);
            lv_obj_set_style_bg_opa(btn_area, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(btn_area, 0, 0);
            lv_obj_set_style_pad_all(btn_area, 0, 0);
            lv_obj_clear_flag(btn_area, LV_OBJ_FLAG_SCROLLABLE);
            _press_fb(btn_area);
            // Antes de abrir cualquier sub-ajuste, recordar la posición de
            // scroll para volver a ella al regresar (registrado antes que el
            // callback real, así se ejecuta primero).
            lv_obj_add_event_cb(btn_area, [](lv_event_t* e){ _settings_remember_scroll(); }, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(btn_area, cb, LV_EVENT_CLICKED, nullptr);
        }
        card_rows++;
        if (card) lv_obj_set_height(card, card_rows * ROW_H);
        y += ROW_H;
    };

    // -- Seguridad --
    section_header(t(K_SEC_SEGURIDAD));
    card_begin();
    {
        int lock_min = vault_get_lock_minutes();
        char lock_str[12];
        if      (lock_min == 0)  snprintf(lock_str, sizeof(lock_str), "%s", t(K_NUNCA));
        else if (lock_min < 0)   snprintf(lock_str, sizeof(lock_str), "%s", t(K_30SEG));
        else if (lock_min == 1)  snprintf(lock_str, sizeof(lock_str), "%s", t(K_1MIN));
        else if (lock_min == 2)  snprintf(lock_str, sizeof(lock_str), "%s", t(K_2MIN));
        else if (lock_min == 5)  snprintf(lock_str, sizeof(lock_str), "%s", t(K_5MIN));
        else                     snprintf(lock_str, sizeof(lock_str), "%s", t(K_30SEG));

        setting_row(ICO_LOCK_CLOCK, t(K_AUTOBLOQUEO), lock_str, false,
            [](lv_event_t* e){
                lv_obj_t* scr_act = lv_scr_act();
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

                const int SH = 264;
                lv_obj_t* sheet = lv_obj_create(overlay);
                lv_obj_set_size(sheet, UI_W, SH);
                lv_obj_set_pos(sheet, 0, UI_H - SH);
                lv_obj_set_style_bg_color(sheet, C_BG, 0);
                lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(sheet, 0, 0);
                lv_obj_set_style_radius(sheet, 12, 0);
                lv_obj_set_style_pad_all(sheet, 0, 0);
                lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_OFF);
                lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

                lv_obj_t* sh_title = lv_label_create(sheet);
                lv_label_set_text(sh_title, t(K_AUTOBLOQUEO));
                lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
                lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
                lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);
                _divider(sheet, 40);

                static const int lk_vals[5] = {-1, 1, 2, 5, 0};
                int cur = vault_get_lock_minutes();

                for (int i = 0; i < 5; i++) {
                    const char* lbl_txt;
                    switch (lk_vals[i]) {
                        case -1: lbl_txt = t(K_30SEG); break;
                        case 1:  lbl_txt = t(K_1MIN);  break;
                        case 2:  lbl_txt = t(K_2MIN);  break;
                        case 5:  lbl_txt = t(K_5MIN);  break;
                        default: lbl_txt = t(K_NUNCA); break;
                    }
                    lv_obj_t* row_l = lv_obj_create(sheet);
                    lv_obj_set_size(row_l, UI_W, 44);
                    lv_obj_set_pos(row_l, 0, 46 + i * 44);
                    lv_obj_set_style_bg_opa(row_l, LV_OPA_TRANSP, 0);
                    lv_obj_set_style_border_width(row_l, 0, 0);
                    lv_obj_set_style_pad_all(row_l, 0, 0);
                    lv_obj_set_style_radius(row_l, 0, 0);
                    lv_obj_clear_flag(row_l, LV_OBJ_FLAG_SCROLLABLE);

                    lv_obj_t* lbl_l = lv_label_create(row_l);
                    lv_label_set_text(lbl_l, lbl_txt);
                    lv_obj_set_style_text_color(lbl_l, C_TEXT, 0);
                    lv_obj_set_style_text_font(lbl_l, FONT_MD, 0);
                    lv_obj_set_pos(lbl_l, 16, 12);
                    lv_obj_clear_flag(lbl_l, LV_OBJ_FLAG_CLICKABLE);

                    if (lk_vals[i] == cur) {
                        lv_obj_t* chk_l = lv_label_create(row_l);
                        lv_label_set_text(chk_l, ICO_CHECK);
                        lv_obj_set_style_text_font(chk_l, FONT_ICON, 0);
                        lv_obj_set_style_text_color(chk_l, C_ACCENT, 0);
                        lv_obj_set_pos(chk_l, UI_W - 40, 10);
                        lv_obj_clear_flag(chk_l, LV_OBJ_FLAG_CLICKABLE);
                    }

                    _press_fb(row_l);
                    lv_obj_add_event_cb(row_l, [](lv_event_t* ev){
                        int idx = (int)(intptr_t)lv_event_get_user_data(ev);
                        lv_obj_t* row = (lv_obj_t*)lv_event_get_target(ev);
                        lv_obj_del(lv_obj_get_parent(lv_obj_get_parent(row)));
                        vault_set_lock_minutes(lk_vals[idx]);
                    }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
                }

                lv_anim_t ash;
                lv_anim_init(&ash); lv_anim_set_var(&ash, sheet);
                lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
                lv_anim_set_values(&ash, UI_H, UI_H - SH);
                lv_anim_set_time(&ash, 220); lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
                lv_anim_start(&ash);
            });
    }
    setting_row(ICO_PIN, t(K_CAMBIAR_PIN), nullptr, false, [](lv_event_t* e){ _show_change_pin(); });
    if (!vault_is_decoy()) {
    setting_row(ICO_PRIVACY,t(K_PIN_SENUELO),
        vault_decoy_is_set() ? t(K_ACTIVO) : t(K_INACTIVO), false,
        [](lv_event_t* e){
            if (!vault_decoy_is_set()) {
                _show_decoy_info();
                return;
            }
            // Mostrar bottom sheet con opciones
            lv_obj_t* ov = lv_obj_create(lv_scr_act());
            lv_obj_set_size(ov, UI_W, UI_H); lv_obj_set_pos(ov, 0, 0);
            lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
            _fade_overlay(ov, LV_OPA_50);
            lv_obj_set_style_border_width(ov, 0, 0); lv_obj_set_style_pad_all(ov, 0, 0);
            lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(ov, [](lv_event_t* ev){
                lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
            }, LV_EVENT_CLICKED, ov);

            const int SH = 260;
            lv_obj_t* sheet = lv_obj_create(ov);
            lv_obj_set_size(sheet, UI_W, SH); lv_obj_set_pos(sheet, 0, UI_H - SH);
            lv_obj_set_style_bg_color(sheet, C_BG, 0); lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(sheet, 0, 0); lv_obj_set_style_radius(sheet, 12, 0);
            lv_obj_set_style_pad_all(sheet, 0, 0);
            lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_OFF);
            lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            // Título
            lv_obj_t* sh_title = lv_label_create(sheet);
            lv_label_set_text(sh_title, t(K_AJUSTES_PIN_SENUELO));
            lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
            lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
            lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);
            _divider(sheet, 40);

            // Cambiar PIN
            lv_obj_t* row_chg = lv_obj_create(sheet);
            lv_obj_set_size(row_chg, UI_W, 44); lv_obj_set_pos(row_chg, 0, 42);
            lv_obj_set_style_bg_opa(row_chg, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row_chg, 0, 0);
            lv_obj_set_style_pad_all(row_chg, 0, 0); lv_obj_clear_flag(row_chg, LV_OBJ_FLAG_SCROLLABLE);
            _press_fb(row_chg);
            lv_obj_t* ico_chg = lv_label_create(row_chg);
            lv_label_set_text(ico_chg, ICO_PIN); lv_obj_set_style_text_font(ico_chg, FONT_ICON, 0);
            lv_obj_set_style_text_color(ico_chg, C_TEXT, 0); lv_obj_set_pos(ico_chg, 16, 14);
            lv_obj_clear_flag(ico_chg, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t* lbl_chg = lv_label_create(row_chg);
            lv_label_set_text(lbl_chg, t(K_CAMBIAR_PIN_SENUELO));
            lv_obj_set_style_text_color(lbl_chg, C_TEXT, 0);
            lv_obj_set_style_text_font(lbl_chg, FONT_MD, 0);
            lv_obj_set_pos(lbl_chg, 44, 14); lv_obj_clear_flag(lbl_chg, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row_chg, [](lv_event_t* ev){
                lv_obj_del(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev)));
                vault_decoy_clear_pin();
                _show_decoy_pin_setup();
            }, LV_EVENT_CLICKED, nullptr);

            _divider(sheet, 86);

            // Selector de tiempo
            lv_obj_t* lbl_time = lv_label_create(sheet);
            lv_label_set_text(lbl_time, t(K_TIEMPO_AUTO_RESET));
            lv_obj_set_style_text_color(lbl_time, C_MUTED, 0);
            lv_obj_set_style_text_font(lbl_time, FONT_SM, 0);
            lv_obj_set_pos(lbl_time, 16, 96);

            int cur_time = vault_decoy_get_reset_time(); // en minutos, default 5
            const int times[] = {2, 5, 10};
            const char* time_labels[] = {t(K_2MIN), t(K_5MIN), t(K_10MIN)};
            int btn_tw = (UI_W - 32 - 16) / 3;

            for (int i = 0; i < 3; i++) {
                lv_obj_t* tb = lv_btn_create(sheet);
                lv_obj_set_size(tb, btn_tw, 34);
                lv_obj_set_pos(tb, 16 + i * (btn_tw + 8), 116);
                bool selected = (cur_time == times[i]);
                lv_obj_set_style_bg_color(tb, selected ? C_ACCENT : C_CARD, 0);
                lv_obj_set_style_radius(tb, 8, 0);
                lv_obj_set_style_shadow_width(tb, 0, 0); lv_obj_set_style_border_width(tb, 0, 0);
                _press_fb(tb);
                lv_obj_t* tl = lv_label_create(tb);
                lv_label_set_text(tl, time_labels[i]);
                lv_obj_set_style_text_color(tl, selected ? lv_color_hex(0xFFFFFF) : C_TEXT, 0);
                lv_obj_set_style_text_font(tl, FONT_SM, 0); lv_obj_center(tl);
                lv_obj_clear_flag(tl, LV_OBJ_FLAG_CLICKABLE);

                static int _time_vals[3] = {2, 5, 10};
                lv_obj_add_event_cb(tb, [](lv_event_t* ev){
                    int t = (int)(intptr_t)lv_event_get_user_data(ev);
                    vault_decoy_set_reset_time(t);
                    // Actualizar colores de los 3 botones
                    lv_obj_t* row = lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev));
                    int times2[] = {2, 5, 10};
                    for (int j = 0; j < 3; j++) {
                        lv_obj_t* btn_j = lv_obj_get_child(row, 3 + j); // 3 = offset por otros elementos
                        if (!btn_j || !lv_obj_is_valid(btn_j)) continue;
                        bool sel = (times2[j] == t);
                        lv_obj_set_style_bg_color(btn_j, sel ? C_ACCENT : C_CARD, 0);
                        lv_obj_t* lbl_j = lv_obj_get_child(btn_j, 0);
                        if (lbl_j) lv_obj_set_style_text_color(lbl_j, sel ? lv_color_hex(0xFFFFFF) : C_TEXT, 0);
                    }
                }, LV_EVENT_CLICKED, (void*)(intptr_t)times[i]);
            }

            _divider(sheet, 162);

            // Desactivar
            lv_obj_t* row_dis = lv_obj_create(sheet);
            lv_obj_set_size(row_dis, UI_W, 44); lv_obj_set_pos(row_dis, 0, 164);
            lv_obj_set_style_bg_opa(row_dis, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row_dis, 0, 0);
            lv_obj_set_style_pad_all(row_dis, 0, 0); lv_obj_clear_flag(row_dis, LV_OBJ_FLAG_SCROLLABLE);
            _press_fb(row_dis);
            lv_obj_t* ico_dis = lv_label_create(row_dis);
            lv_label_set_text(ico_dis, ICO_DELETE); lv_obj_set_style_text_font(ico_dis, FONT_ICON, 0);
            lv_obj_set_style_text_color(ico_dis, C_DANGER, 0); lv_obj_set_pos(ico_dis, 16, 14);
            lv_obj_clear_flag(ico_dis, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t* lbl_dis = lv_label_create(row_dis);
            lv_label_set_text(lbl_dis, t(K_DESACTIVAR_PIN_SENUELO));
            lv_obj_set_style_text_color(lbl_dis, C_DANGER, 0);
            lv_obj_set_style_text_font(lbl_dis, FONT_MD, 0);
            lv_obj_set_pos(lbl_dis, 44, 14); lv_obj_clear_flag(lbl_dis, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row_dis, [](lv_event_t* ev){
                lv_obj_del(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev)));
                vault_decoy_clear_pin();
                _show_settings();
            }, LV_EVENT_CLICKED, nullptr);

            // Animación entrada
            lv_anim_t ash;
            lv_anim_init(&ash); lv_anim_set_var(&ash, sheet);
            lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
            lv_anim_set_values(&ash, UI_H, UI_H - SH);
            lv_anim_set_time(&ash, 220); lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
            lv_anim_start(&ash);
        });
    } // if (!vault_is_decoy())
    if (!vault_is_decoy()) {
        int dm_days = vault_get_deadman_days();
        setting_row(ICO_WARNING, t(K_MODO_PERDIDO),
            dm_days > 0 ? t(K_ACTIVO) : t(K_INACTIVO), false,
            [](lv_event_t* e){
                if (vault_get_deadman_days() <= 0) { _show_deadman_info(); return; }
                _show_deadman_settings_sheet();
            });
        int sn_lvl = vault_get_snatch_level();
        setting_row(ICO_SHIELD, t(K_SNATCH_LOCK),
            sn_lvl > 0 ? t(K_ACTIVO) : t(K_INACTIVO), false,
            [](lv_event_t* e){
                if (vault_get_snatch_level() <= 0) { _show_snatch_info(); return; }
                _show_snatch_settings_sheet();
            });
    }
    card_end();

    // -- General --
    section_header(t(K_SEC_GENERAL));
    card_begin();
    setting_row(_current_theme == THEME_PERSONALIZADO ? ICO_SETTINGS : (_current_theme == THEME_OSCURO ? ICO_DARK_MODE : ICO_LIGHT_MODE),
        t(K_PERSONALIZACION),
        _current_theme == THEME_PERSONALIZADO ? t(K_PERSONALIZADO) : (_current_theme == THEME_OSCURO ? t(K_OSCURO) : t(K_CLARO)), false,
        [](lv_event_t* e){ _show_personalization(); });
    setting_row(ICO_LANGUAGE, t(K_IDIOMA),
        i18n_get_lang() == LANG_ES ? "Español" : "English", false,
        [](lv_event_t* e){
            lv_obj_t* scr_act = lv_scr_act();

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

            const int SH = 168;
            lv_obj_t* sheet = lv_obj_create(overlay);
            lv_obj_set_size(sheet, UI_W, SH);
            lv_obj_set_pos(sheet, 0, UI_H - SH);
            lv_obj_set_style_bg_color(sheet, C_BG, 0);
            lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(sheet, 0, 0);
            lv_obj_set_style_radius(sheet, 12, 0);
            lv_obj_set_style_pad_all(sheet, 0, 0);
            lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_OFF);
            lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(sheet, [](lv_event_t* ev){
                lv_event_stop_bubbling(ev);
            }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* sh_title = lv_label_create(sheet);
            lv_label_set_text(sh_title, t(K_IDIOMA));
            lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
            lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
            lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);

            _divider(sheet, 40);

            const char* lang_names[LANG_COUNT] = { "Español", "English" };
            for (int i = 0; i < LANG_COUNT; i++) {
                lv_obj_t* row_l = lv_obj_create(sheet);
                lv_obj_set_size(row_l, UI_W, 44);
                lv_obj_set_pos(row_l, 0, 46 + i * 44);
                lv_obj_set_style_bg_opa(row_l, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(row_l, 0, 0);
                lv_obj_set_style_pad_all(row_l, 0, 0);
                lv_obj_set_style_radius(row_l, 0, 0);
                lv_obj_clear_flag(row_l, LV_OBJ_FLAG_SCROLLABLE);

                lv_obj_t* lbl_l = lv_label_create(row_l);
                lv_label_set_text(lbl_l, lang_names[i]);
                lv_obj_set_style_text_color(lbl_l, C_TEXT, 0);
                lv_obj_set_style_text_font(lbl_l, FONT_MD, 0);
                lv_obj_set_pos(lbl_l, 16, 12);
                lv_obj_clear_flag(lbl_l, LV_OBJ_FLAG_CLICKABLE);

                if (i == i18n_get_lang()) {
                    lv_obj_t* chk_l = lv_label_create(row_l);
                    lv_label_set_text(chk_l, ICO_CHECK);
                    lv_obj_set_style_text_font(chk_l, FONT_ICON, 0);
                    lv_obj_set_style_text_color(chk_l, C_ACCENT, 0);
                    lv_obj_set_pos(chk_l, UI_W - 40, 10);
                    lv_obj_clear_flag(chk_l, LV_OBJ_FLAG_CLICKABLE);
                }

                _press_fb(row_l);
                lv_obj_add_event_cb(row_l, [](lv_event_t* ev){
                    int idx = (int)(intptr_t)lv_event_get_user_data(ev);
                    lv_obj_t* row = (lv_obj_t*)lv_event_get_target(ev);
                    lv_obj_del(lv_obj_get_parent(lv_obj_get_parent(row))); // borra overlay completo
                    i18n_set_lang(idx);
                    _show_settings(true);
                }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            }

            // Animación entrada
            lv_anim_t ash;
            lv_anim_init(&ash); lv_anim_set_var(&ash, sheet);
            lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
            lv_anim_set_values(&ash, UI_H, UI_H - SH);
            lv_anim_set_time(&ash, 220); lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
            lv_anim_start(&ash);
        });
    const char* kb_names[] = {"ES-LAT", "ES-ES", "US"};
    int cur_kb = vault_get_kb_layout();
    if (cur_kb < 0 || cur_kb > 2) cur_kb = 0;
    // Modo teclado (USB / Bluetooth)
    setting_row(vault_get_kb_mode() == 1 ? ICO_BLUETOOTH : ICO_USB, t(K_MODO_TECLADO),
        vault_get_kb_mode() == 1 ? "Bluetooth" : "USB", false,
        [](lv_event_t* e){
            lv_obj_t* overlay = lv_obj_create(lv_scr_act());
            lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
            _fade_overlay(overlay, LV_OPA_50);
            lv_obj_set_style_border_width(overlay, 0, 0); lv_obj_set_style_pad_all(overlay, 0, 0);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(overlay, [](lv_event_t* ev){
                lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
            }, LV_EVENT_CLICKED, overlay);

            const int SH = 160;
            lv_obj_t* sheet = lv_obj_create(overlay);
            lv_obj_set_size(sheet, UI_W, SH); lv_obj_set_pos(sheet, 0, UI_H - SH);
            lv_obj_set_style_bg_color(sheet, C_BG, 0); lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(sheet, 0, 0); lv_obj_set_style_radius(sheet, 12, 0);
            lv_obj_set_style_pad_all(sheet, 0, 0); lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* title = lv_label_create(sheet);
            lv_label_set_text(title, t(K_MODO_TECLADO));
            lv_obj_set_style_text_font(title, FONT_HDR, 0);
            lv_obj_set_style_text_color(title, C_TEXT, 0);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);
            _divider(sheet, 40);

            static const char* opts[2] = {"USB", "Bluetooth"};
            int cur_mode = vault_get_kb_mode();
            for (int i = 0; i < 2; i++) {
                lv_obj_t* row = lv_obj_create(sheet);
                lv_obj_set_size(row, UI_W, 50); lv_obj_set_pos(row, 0, 46 + i * 50);
                lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(row, 0, 0); lv_obj_set_style_pad_all(row, 0, 0);
                lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_t* lbl = lv_label_create(row);
                lv_label_set_text(lbl, opts[i]);
                lv_obj_set_style_text_font(lbl, FONT_MD, 0);
                lv_obj_set_style_text_color(lbl, C_TEXT, 0);
                lv_obj_set_pos(lbl, 16, 16);
                lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
                if (i == cur_mode) {
                    lv_obj_t* chk = lv_label_create(row);
                    lv_label_set_text(chk, ICO_CHECK);
                    lv_obj_set_style_text_font(chk, FONT_ICON, 0);
                    lv_obj_set_style_text_color(chk, C_ACCENT, 0);
                    lv_obj_set_pos(chk, UI_W - 36, 16);
                    lv_obj_clear_flag(chk, LV_OBJ_FLAG_CLICKABLE);
                }
                _press_fb(row);
                lv_obj_add_event_cb(row, [](lv_event_t* ev){
                    int idx = (int)(intptr_t)lv_event_get_user_data(ev);
                    lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
                    if (idx != vault_get_kb_mode()) _show_kb_mode_warning(idx);
                }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            }

            lv_anim_t ash; lv_anim_init(&ash); lv_anim_set_var(&ash, sheet);
            lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
            lv_anim_set_values(&ash, UI_H, UI_H - SH);
            lv_anim_set_time(&ash, 220); lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
            lv_anim_start(&ash);
        });
    setting_row(ICO_KEYBOARD, t(K_LAYOUT_TECLADO),
        kb_names[cur_kb], false,
        [](lv_event_t* e){
            lv_obj_t* scr_act = lv_scr_act();

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

            const int SH = 200;
            lv_obj_t* sheet = lv_obj_create(overlay);
            lv_obj_set_size(sheet, UI_W, SH);
            lv_obj_set_pos(sheet, 0, UI_H - SH);
            lv_obj_set_style_bg_color(sheet, C_BG, 0);
            lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(sheet, 0, 0);
            lv_obj_set_style_radius(sheet, 12, 0);
            lv_obj_set_style_pad_all(sheet, 0, 0);
            lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_OFF);
            lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(sheet, [](lv_event_t* ev){
                lv_event_stop_bubbling(ev);
            }, LV_EVENT_CLICKED, nullptr);



            // Título
            lv_obj_t* sh_title = lv_label_create(sheet);
            lv_label_set_text(sh_title, t(K_TECLADO_USB));
            lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
            lv_obj_set_style_text_font(sh_title, FONT_MD, 0);
            lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 20);
            // Divider
            lv_obj_t* div = lv_obj_create(sheet);
            lv_obj_set_size(div, UI_W, 1); lv_obj_set_pos(div, 0, 38);
            lv_obj_set_style_bg_color(div, C_BORDER, 0); lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(div, 0, 0); lv_obj_set_style_pad_all(div, 0, 0);

            // Opciones
            struct KbOpt { const char* label; const char* desc; int mode; };
            const KbOpt opts[] = {
                { "ES-LAT", t(K_KB_MX), 0 },
                { "ES-ES",  t(K_KB_ES),                 1 },
                { "US",     t(K_KB_INTL), 2 },
            };
            static int kb_sel_modes[3];
            int cur = vault_get_kb_layout();

            for (int i = 0; i < 3; i++) {
                lv_obj_t* row = lv_obj_create(sheet);
                lv_obj_set_size(row, UI_W, 44);
                lv_obj_set_pos(row, 0, 46 + i * 44);
                lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(row, 0, 0);
                lv_obj_set_style_pad_all(row, 0, 0);
                lv_obj_set_style_radius(row, 0, 0);
                lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

                bool sel = (cur == opts[i].mode);

                lv_obj_t* lbl = lv_label_create(row);
                lv_label_set_text(lbl, opts[i].label);
                lv_obj_set_style_text_font(lbl, FONT_SM, 0);
                lv_obj_set_style_text_color(lbl, sel ? C_BTN_BG : C_TEXT, 0);
                lv_obj_set_pos(lbl, 16, 8);

                lv_obj_t* desc = lv_label_create(row);
                lv_label_set_text(desc, opts[i].desc);
                lv_obj_set_style_text_font(desc, FONT_SM, 0);
                lv_obj_set_style_text_color(desc, C_MUTED, 0);
                lv_obj_set_pos(desc, 16, 24);

                if (sel) {
                    lv_obj_t* chk = lv_label_create(row);
                    lv_label_set_text(chk, ICO_CHECK); lv_obj_set_style_text_font(chk, FONT_ICON, 0);
                    lv_obj_set_style_text_color(chk, C_BTN_BG, 0);
                    lv_obj_set_style_text_font(chk, FONT_ICON, 0);
                    lv_obj_set_pos(chk, UI_W - 28, 16);
                }

                kb_sel_modes[i] = opts[i].mode;
                lv_obj_add_event_cb(row, [](lv_event_t* ev){
                    int idx = (int)(intptr_t)lv_event_get_user_data(ev);
                    vault_set_kb_layout(kb_sel_modes[idx]);
                    // Actualizar label del teclado in-place
                    lv_obj_t* overlay_obj = lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev));
                    // Encontrar el setting_row del teclado en ajustes
                    lv_obj_t* scr_obj = lv_scr_act();
                    lv_obj_del(overlay_obj);
                    // Recargar solo para actualizar el check — aceptable porque es una sola vez
                    _show_settings();
                }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            }

            // Animación
            lv_anim_t ash;
            lv_anim_init(&ash);
            lv_anim_set_var(&ash, sheet);
            lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
            lv_anim_set_values(&ash, UI_H, UI_H - SH);
            lv_anim_set_time(&ash, 220);
            lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
            lv_anim_start(&ash);
        });
    setting_row(ICO_KEYBOARD, t(K_TIPO_TECLADO), nullptr, false,
        [](lv_event_t* e){
            lv_obj_t* overlay = lv_obj_create(lv_scr_act());
            lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
            _fade_overlay(overlay, LV_OPA_50);
            lv_obj_set_style_border_width(overlay, 0, 0); lv_obj_set_style_pad_all(overlay, 0, 0);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(overlay, [](lv_event_t* ev){
                lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
            }, LV_EVENT_CLICKED, overlay);

            const int SH = 160;
            lv_obj_t* sheet = lv_obj_create(overlay);
            lv_obj_set_size(sheet, UI_W, SH); lv_obj_set_pos(sheet, 0, UI_H - SH);
            lv_obj_set_style_bg_color(sheet, C_BG, 0); lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(sheet, 0, 0); lv_obj_set_style_radius(sheet, 12, 0);
            lv_obj_set_style_pad_all(sheet, 0, 0); lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* title = lv_label_create(sheet);
            lv_label_set_text(title, t(K_TIPO_TECLADO));
            lv_obj_set_style_text_font(title, FONT_HDR, 0);
            lv_obj_set_style_text_color(title, C_TEXT, 0);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);
            _divider(sheet, 40);

            static const char* opts[2] = {"Deslizante", "T9"};
            bool cur_t9 = vault_get_t9_mode();
            for (int i = 0; i < 2; i++) {
                lv_obj_t* row = lv_obj_create(sheet);
                lv_obj_set_size(row, UI_W, 50); lv_obj_set_pos(row, 0, 46 + i * 50);
                lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(row, 0, 0); lv_obj_set_style_pad_all(row, 0, 0);
                lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_t* lbl = lv_label_create(row);
                lv_label_set_text(lbl, opts[i]);
                lv_obj_set_style_text_font(lbl, FONT_MD, 0);
                lv_obj_set_style_text_color(lbl, C_TEXT, 0);
                lv_obj_set_pos(lbl, 16, 16);
                lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
                if ((i == 1) == cur_t9) {
                    lv_obj_t* chk = lv_label_create(row);
                    lv_label_set_text(chk, ICO_CHECK);
                    lv_obj_set_style_text_font(chk, FONT_ICON, 0);
                    lv_obj_set_style_text_color(chk, C_ACCENT, 0);
                    lv_obj_set_pos(chk, UI_W - 36, 16);
                    lv_obj_clear_flag(chk, LV_OBJ_FLAG_CLICKABLE);
                }
                _press_fb(row);
                lv_obj_add_event_cb(row, [](lv_event_t* ev){
                    int idx = (int)(intptr_t)lv_event_get_user_data(ev);
                    vault_set_t9_mode(idx == 1);
                    lv_obj_del(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev)));
                    _show_settings(true);
                }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            }

            lv_anim_t ash; lv_anim_init(&ash); lv_anim_set_var(&ash, sheet);
            lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
            lv_anim_set_values(&ash, UI_H, UI_H - SH);
            lv_anim_set_time(&ash, 220); lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
            lv_anim_start(&ash);
        });
    card_end();

    // -- Conectividad --
    section_header(t(K_SEC_CONECTIVIDAD));
    card_begin();
    setting_row(ICO_USB, t(K_MODO_USB), nullptr, true,
        [](lv_event_t* e){
            lv_obj_t* overlay = lv_obj_create(lv_scr_act());
            lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
            _fade_overlay(overlay, LV_OPA_60);
            lv_obj_set_style_border_width(overlay, 0, 0);
            lv_obj_set_style_pad_all(overlay, 0, 0);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* box = lv_obj_create(overlay);
            lv_obj_set_size(box, UI_W - 48, 232);
            lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_bg_color(box, C_CARD, 0); lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(box, 0, 0); lv_obj_set_style_radius(box, 12, 0);
            lv_obj_set_style_pad_all(box, 16, 0);
            lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(box, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* msg = lv_label_create(box);
            lv_label_set_text(msg, t(K_USB_CONFIRM_MSG));
            lv_obj_set_style_text_color(msg, C_TEXT, 0);
            lv_obj_set_style_text_font(msg, FONT_MD, 0);
            lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 0);

            // Opción de solo lectura para almacenamiento USB
            lv_obj_t* ro_lbl = lv_label_create(box);
            lv_label_set_text(ro_lbl, t(K_USB_SOLO_LECTURA));
            lv_obj_set_style_text_color(ro_lbl, C_TEXT, 0);
            lv_obj_set_style_text_font(ro_lbl, FONT_SM, 0);
            lv_obj_set_width(ro_lbl, UI_W - 48 - 32 - 54); // no invadir el switch
            lv_label_set_long_mode(ro_lbl, LV_LABEL_LONG_WRAP);
            lv_obj_set_pos(ro_lbl, 0, 128);

            lv_obj_t* ro_sw = lv_switch_create(box);
            lv_obj_set_size(ro_sw, 46, 26);
            lv_obj_align(ro_sw, LV_ALIGN_TOP_RIGHT, 0, 124);
            lv_obj_set_style_bg_color(ro_sw, C_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
            if (vault_get_usb_msc_readonly()) lv_obj_add_state(ro_sw, LV_STATE_CHECKED);
            lv_obj_add_event_cb(ro_sw, [](lv_event_t* ev){
                lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(ev);
                vault_set_usb_msc_readonly(lv_obj_has_state(sw, LV_STATE_CHECKED));
            }, LV_EVENT_VALUE_CHANGED, nullptr);

            int bw = (UI_W - 48 - 32 - 8) / 2;
            lv_obj_t* btn_no = lv_btn_create(box);
            _press_fb(btn_no);
            lv_obj_set_size(btn_no, bw, 36); lv_obj_set_pos(btn_no, 0, 168);
            lv_obj_set_style_bg_color(btn_no, C_BORDER, 0); lv_obj_set_style_radius(btn_no, 8, 0);
            lv_obj_set_style_shadow_width(btn_no, 0, 0); lv_obj_set_style_border_width(btn_no, 1, 0);
            lv_obj_set_style_border_color(btn_no, C_MUTED, 0);
            lv_obj_t* no_lbl = lv_label_create(btn_no); lv_label_set_text(no_lbl, t(K_CANCELAR));
            lv_obj_set_style_text_color(no_lbl, C_TEXT, 0); lv_obj_set_style_text_font(no_lbl, FONT_SM, 0);
            lv_obj_center(no_lbl);
            lv_obj_add_event_cb(btn_no, [](lv_event_t* e){
                lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e))));
            }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* btn_ok = lv_btn_create(box);
            _press_fb(btn_ok);
            lv_obj_set_size(btn_ok, bw, 36); lv_obj_set_pos(btn_ok, bw + 8, 168);
            lv_obj_set_style_bg_color(btn_ok, C_DANGER, 0); lv_obj_set_style_radius(btn_ok, 8, 0);
            lv_obj_set_style_shadow_width(btn_ok, 0, 0); lv_obj_set_style_border_width(btn_ok, 0, 0);
            lv_obj_t* ok_lbl = lv_label_create(btn_ok); lv_label_set_text(ok_lbl, t(K_ACTIVAR));
            lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ok_lbl, FONT_SM, 0); lv_obj_center(ok_lbl);
            lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
                vault_set_usb_msc_mode(true);
                Serial.println("[APP] Modo USB activado, reiniciando...");
                lv_obj_t* ov1 = lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e)));
                lv_obj_t* msg2 = lv_label_create(ov1);
                lv_label_set_text(msg2, t(K_REINICIANDO));
                lv_obj_set_style_text_color(msg2, lv_color_hex(0xFFFFFF), 0);
                lv_obj_center(msg2);
                lv_timer_create([](lv_timer_t* t){ ESP.restart(); }, 300, nullptr);
            }, LV_EVENT_CLICKED, nullptr);
        });

    setting_row(ICO_BLUETOOTH, t(K_BLE_OLVIDAR), nullptr, true,
        [](lv_event_t* e){
            lv_obj_t* overlay = lv_obj_create(lv_scr_act());
            lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
            _fade_overlay(overlay, LV_OPA_60);
            lv_obj_set_style_border_width(overlay, 0, 0);
            lv_obj_set_style_pad_all(overlay, 0, 0);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(overlay, [](lv_event_t* ev){
                lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
            }, LV_EVENT_CLICKED, overlay);

            lv_obj_t* box = lv_obj_create(overlay);
            lv_obj_set_size(box, UI_W - 48, 200);
            lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_bg_color(box, C_CARD, 0); lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(box, 0, 0); lv_obj_set_style_radius(box, 12, 0);
            lv_obj_set_style_pad_all(box, 16, 0);
            lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(box, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* msg = lv_label_create(box);
            lv_label_set_text(msg, t(K_BLE_OLVIDAR_CONFIRM));
            lv_obj_set_style_text_color(msg, C_TEXT, 0);
            lv_obj_set_style_text_font(msg, FONT_SM, 0);
            lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 0);

            int bw = (UI_W - 48 - 32 - 8) / 2;
            lv_obj_t* btn_no = lv_btn_create(box);
            _press_fb(btn_no);
            lv_obj_set_size(btn_no, bw, 36); lv_obj_set_pos(btn_no, 0, 130);
            lv_obj_set_style_bg_color(btn_no, C_BORDER, 0); lv_obj_set_style_radius(btn_no, 8, 0);
            lv_obj_set_style_shadow_width(btn_no, 0, 0); lv_obj_set_style_border_width(btn_no, 1, 0);
            lv_obj_set_style_border_color(btn_no, C_MUTED, 0);
            lv_obj_t* no_lbl = lv_label_create(btn_no); lv_label_set_text(no_lbl, t(K_CANCELAR));
            lv_obj_set_style_text_color(no_lbl, C_TEXT, 0); lv_obj_set_style_text_font(no_lbl, FONT_SM, 0);
            lv_obj_center(no_lbl);
            lv_obj_add_event_cb(btn_no, [](lv_event_t* ev){
                lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
            }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* btn_ok = lv_btn_create(box);
            _press_fb(btn_ok);
            lv_obj_set_size(btn_ok, bw, 36); lv_obj_set_pos(btn_ok, bw + 8, 130);
            lv_obj_set_style_bg_color(btn_ok, C_DANGER, 0); lv_obj_set_style_radius(btn_ok, 8, 0);
            lv_obj_set_style_shadow_width(btn_ok, 0, 0); lv_obj_set_style_border_width(btn_ok, 0, 0);
            lv_obj_t* ok_lbl = lv_label_create(btn_ok); lv_label_set_text(ok_lbl, t(K_ELIMINAR));
            lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ok_lbl, FONT_SM, 0); lv_obj_center(ok_lbl);
            lv_obj_add_event_cb(btn_ok, [](lv_event_t* ev){
                lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
                bool ok = ble_kb_delete_all_bonds();
                if (ok) toast_ok(t(K_TOAST_BONDS_BORRADOS));
                else    toast_err(t(K_ERROR));
            }, LV_EVENT_CLICKED, nullptr);
        });
    setting_row(ICO_DOWNLOAD, t(K_IMPORTAR_EXPORTAR), nullptr, false, [](lv_event_t* e){ _show_import_export(); });
    
    // -- Zona horaria (offset UTC para 2FA/TOTP) --
    card_end();
    card_begin();
    {
        int off = vault_get_tz_offset_min();
        char off_str[16];
        snprintf(off_str, sizeof(off_str), "UTC%+d:%02d", off / 60, abs(off % 60));
        setting_row(ICO_LANGUAGE, t(K_ZONA_HORARIA), off_str, false, [](lv_event_t* e){
            lv_obj_t* overlay = lv_obj_create(lv_scr_act());
            lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
            _fade_overlay(overlay, LV_OPA_50);
            lv_obj_set_style_border_width(overlay, 0, 0); lv_obj_set_style_pad_all(overlay, 0, 0);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(overlay, [](lv_event_t* ev){
                lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
            }, LV_EVENT_CLICKED, overlay);

            const int SH = 220;
            lv_obj_t* sheet = lv_obj_create(overlay);
            lv_obj_set_size(sheet, UI_W, SH); lv_obj_set_pos(sheet, 0, UI_H - SH);
            lv_obj_set_style_bg_color(sheet, C_BG, 0); lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(sheet, 0, 0); lv_obj_set_style_radius(sheet, 12, 0);
            lv_obj_set_style_pad_all(sheet, 0, 0);
            lv_obj_add_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_scroll_dir(sheet, LV_DIR_VER);
            lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* title = lv_label_create(sheet);
            lv_label_set_text(title, t(K_ZONA_HORARIA));
            lv_obj_set_style_text_font(title, FONT_HDR, 0);
            lv_obj_set_style_text_color(title, C_TEXT, 0);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);
            _divider(sheet, 40);

            int cur_off = vault_get_tz_offset_min();
            for (int m = -720; m <= 840; m += 30) { // UTC-12 a UTC+14
                lv_obj_t* row = lv_obj_create(sheet);
                lv_obj_set_size(row, UI_W, 44); lv_obj_set_pos(row, 0, 46 + ((m + 720) / 30) * 44);
                lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(row, 0, 0); lv_obj_set_style_pad_all(row, 0, 0);
                lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_t* lbl = lv_label_create(row);
                char buf[16]; snprintf(buf, sizeof(buf), "UTC%+d:%02d", m / 60, abs(m % 60));
                lv_label_set_text(lbl, buf);
                lv_obj_set_style_text_font(lbl, FONT_MD, 0);
                lv_obj_set_style_text_color(lbl, C_TEXT, 0);
                lv_obj_set_pos(lbl, 16, 12);
                lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
                if (m == cur_off) {
                    lv_obj_t* chk = lv_label_create(row);
                    lv_label_set_text(chk, ICO_CHECK);
                    lv_obj_set_style_text_font(chk, FONT_ICON, 0);
                    lv_obj_set_style_text_color(chk, C_ACCENT, 0);
                    lv_obj_set_pos(chk, UI_W - 36, 10);
                    lv_obj_clear_flag(chk, LV_OBJ_FLAG_CLICKABLE);
                }
                _press_fb(row);
                lv_obj_add_event_cb(row, [](lv_event_t* ev){
                    int val = (int)(intptr_t)lv_event_get_user_data(ev);
                    vault_set_tz_offset_min(val);
                    lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
                    _show_settings(false);
                }, LV_EVENT_CLICKED, (void*)(intptr_t)m);
            }
        });
    }
    card_end();

    // -- Dispositivo --
    section_header(t(K_SEC_DISPOSITIVO));
    card_begin();
    setting_row(ICO_INFO,     t(K_INFORMACION),        nullptr, false, [](lv_event_t* e){ _show_info(); });
    setting_row(ICO_WARNING,  t(K_AVISO_PRIVACIDAD),nullptr, false, [](lv_event_t* e){ _show_privacy(); });
    setting_row(ICO_DOWNLOAD, t(K_ACERCA_DE),          nullptr, false, [](lv_event_t* e){ _show_about(); });
    card_end();

    // -- Sistema --
    section_header(t(K_SEC_SISTEMA));
    card_begin();
    setting_row(ICO_CALENDAR, t(K_FECHA), nullptr, false, [](lv_event_t* e){ _show_date_settings(); });
    setting_row(ICO_SCHEDULE, t(K_HORA),  nullptr, false, [](lv_event_t* e){ _show_time_settings(); });
    setting_row(ICO_DELETE, t(K_RESTABLECER_FABRICA), nullptr, true,
        [](lv_event_t* e){
            // Confirmación inicial
            lv_obj_t* overlay = lv_obj_create(lv_scr_act());
            lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
            _fade_overlay(overlay, LV_OPA_60);
            lv_obj_set_style_border_width(overlay, 0, 0);
            lv_obj_set_style_pad_all(overlay, 0, 0);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* box = lv_obj_create(overlay);
            lv_obj_set_size(box, UI_W - 48, 170);
            lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_bg_color(box, C_CARD, 0); lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(box, 0, 0);
            lv_obj_set_style_radius(box, 12, 0);
            lv_obj_set_style_pad_all(box, 16, 0);
            lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(box, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* msg = lv_label_create(box);
            lv_label_set_text(msg, t(K_RESET_FABRICA_MSG));
            lv_obj_set_style_text_color(msg, C_TEXT, 0);
            lv_obj_set_style_text_font(msg, FONT_MD, 0);
            lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 0);

            int bw = (UI_W - 48 - 32 - 8) / 2;
            // Cancelar
            lv_obj_t* btn_no = lv_btn_create(box);
            _press_fb(btn_no);
            lv_obj_set_size(btn_no, bw, 36); lv_obj_set_pos(btn_no, 0, 106);
            lv_obj_set_style_bg_color(btn_no, C_BORDER, 0); lv_obj_set_style_radius(btn_no, 8, 0);
            lv_obj_set_style_shadow_width(btn_no, 0, 0); lv_obj_set_style_border_width(btn_no, 1, 0);
            lv_obj_set_style_border_color(btn_no, C_MUTED, 0);
            lv_obj_t* no_lbl = lv_label_create(btn_no); lv_label_set_text(no_lbl, t(K_CANCELAR));
            lv_obj_set_style_text_color(no_lbl, C_TEXT, 0); lv_obj_set_style_text_font(no_lbl, FONT_SM, 0);
            lv_obj_center(no_lbl);
            lv_obj_add_event_cb(btn_no, [](lv_event_t* e){
                lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e))));
            }, LV_EVENT_CLICKED, nullptr);

            // Continuar
            lv_obj_t* btn_ok = lv_btn_create(box);
            _press_fb(btn_ok);
            lv_obj_set_size(btn_ok, bw, 36); lv_obj_set_pos(btn_ok, bw + 8, 106);
            lv_obj_set_style_bg_color(btn_ok, C_DANGER, 0); lv_obj_set_style_radius(btn_ok, 8, 0);
            lv_obj_set_style_shadow_width(btn_ok, 0, 0); lv_obj_set_style_border_width(btn_ok, 0, 0);
            lv_obj_t* ok_lbl = lv_label_create(btn_ok); lv_label_set_text(ok_lbl, t(K_CONTINUAR));
            lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ok_lbl, FONT_SM, 0); lv_obj_center(ok_lbl);

            lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
                // Cerrar popup de confirmación
                lv_obj_t* ov1 = lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e)));
                lv_obj_del(ov1);

                // Confirmación por palabra clave ("Reset")
                lv_obj_t* pin_overlay = lv_obj_create(lv_scr_act());
                lv_obj_set_size(pin_overlay, UI_W, UI_H); lv_obj_set_pos(pin_overlay, 0, 0);
                lv_obj_set_style_bg_color(pin_overlay, lv_color_hex(0x000000), 0);
                lv_obj_set_style_bg_opa(pin_overlay, LV_OPA_90, 0);
                lv_obj_set_style_border_width(pin_overlay, 0, 0);
                lv_obj_set_style_pad_all(pin_overlay, 0, 0);
                lv_obj_clear_flag(pin_overlay, LV_OBJ_FLAG_SCROLLABLE);

                // Cancelar
                lv_obj_t* btn_rcx = lv_btn_create(pin_overlay);
                _press_fb(btn_rcx);
                lv_obj_set_size(btn_rcx, 44, 44); lv_obj_set_pos(btn_rcx, UI_W - 44, 0);
                lv_obj_set_style_bg_opa(btn_rcx, LV_OPA_TRANSP, 0);
                lv_obj_set_style_shadow_width(btn_rcx, 0, 0); lv_obj_set_style_border_width(btn_rcx, 0, 0);
                lv_obj_t* rcx_ico = lv_label_create(btn_rcx);
                lv_label_set_text(rcx_ico, ICO_CLOSE); lv_obj_set_style_text_font(rcx_ico, FONT_ICON, 0);
                lv_obj_set_style_text_color(rcx_ico, C_MUTED, 0); lv_obj_center(rcx_ico);
                lv_obj_add_event_cb(btn_rcx, [](lv_event_t* e){
                    kb_reset();
                    lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
                }, LV_EVENT_CLICKED, pin_overlay);

                lv_obj_t* r_title = lv_label_create(pin_overlay);
                lv_label_set_text(r_title, t(K_CONFIRMACION));
                lv_obj_set_style_text_color(r_title, C_TEXT, 0);
                lv_obj_set_style_text_font(r_title, FONT_HDR, 0);
                lv_obj_align(r_title, LV_ALIGN_TOP_MID, 0, 12);

                lv_obj_t* r_sub = lv_label_create(pin_overlay);
                lv_label_set_text(r_sub, t(K_ESCRIBE_RESET));
                lv_obj_set_style_text_color(r_sub, C_MUTED, 0);
                lv_obj_set_style_text_font(r_sub, FONT_SM, 0);
                lv_obj_align(r_sub, LV_ALIGN_TOP_MID, 0, 40);

                lv_obj_t* r_ta = lv_textarea_create(pin_overlay);
                lv_obj_set_size(r_ta, UI_W - 24, 44); lv_obj_set_pos(r_ta, 12, 64);
                lv_textarea_set_one_line(r_ta, true);
                lv_textarea_set_max_length(r_ta, 10);
                lv_textarea_set_placeholder_text(r_ta, "Reset");
                lv_obj_set_style_bg_color(r_ta, C_CARD, 0); lv_obj_set_style_bg_opa(r_ta, LV_OPA_COVER, 0);
                lv_obj_set_style_border_color(r_ta, C_DANGER, 0);
                lv_obj_set_style_border_width(r_ta, 2, 0); lv_obj_set_style_radius(r_ta, 10, 0);
                lv_obj_set_style_text_color(r_ta, C_TEXT, 0); lv_obj_set_style_text_font(r_ta, FONT_MD, 0);
                lv_obj_set_style_pad_ver(r_ta, 12, 0); lv_obj_set_style_pad_hor(r_ta, 12, 0);
                lv_obj_set_scrollbar_mode(r_ta, LV_SCROLLBAR_MODE_OFF);

                lv_obj_t* r_err = lv_label_create(pin_overlay);
                lv_label_set_text(r_err, "");
                lv_obj_set_style_text_color(r_err, C_DANGER, 0);
                lv_obj_set_style_text_font(r_err, FONT_SM, 0);
                lv_obj_align(r_err, LV_ALIGN_TOP_MID, 0, 114);

                struct ResetWordCtx { lv_obj_t* ta; lv_obj_t* ov; lv_obj_t* err; };
                static ResetWordCtx rwctx;
                rwctx = {r_ta, pin_overlay, r_err};

                lv_obj_t* r_kb = kb_create(pin_overlay, r_ta, []{
                    lv_obj_t* ta = kb_get_ta();
                    if (!ta || !lv_obj_is_valid(ta)) return;
                    const char* txt = lv_textarea_get_text(ta);
                    if (strcmp(txt, "Reset") != 0) {
                        if (rwctx.err && lv_obj_is_valid(rwctx.err))
                            lv_label_set_text(rwctx.err, t(K_ESCRIBE_EXACTAMENTE_RESET));
                        lv_textarea_set_text(ta, "");
                        return;
                    }
                    kb_reset();
                    lv_obj_t* pin_overlay = rwctx.ov;

                    // Progreso de reset de fábrica
                    lv_obj_t* hold_ov = lv_obj_create(lv_scr_act());
                    lv_obj_set_size(hold_ov, UI_W, UI_H); lv_obj_set_pos(hold_ov, 0, 0);
                    lv_obj_set_style_bg_color(hold_ov, lv_color_hex(0x000000), 0);
                    lv_obj_set_style_bg_opa(hold_ov, LV_OPA_90, 0);
                    lv_obj_set_style_border_width(hold_ov, 0, 0);
                    lv_obj_set_style_pad_all(hold_ov, 0, 0);
                    lv_obj_clear_flag(hold_ov, LV_OBJ_FLAG_SCROLLABLE);

                    // ✗ cancelar
                    lv_obj_t* btn_hx = lv_btn_create(hold_ov);
                    _press_fb(btn_hx);
                    lv_obj_set_size(btn_hx, 44, 44); lv_obj_set_pos(btn_hx, UI_W - 44, 0);
                    lv_obj_set_style_bg_opa(btn_hx, LV_OPA_TRANSP, 0);
                    lv_obj_set_style_shadow_width(btn_hx, 0, 0); lv_obj_set_style_border_width(btn_hx, 0, 0);
                    lv_obj_t* hx_ico = lv_label_create(btn_hx);
                    lv_label_set_text(hx_ico, ICO_CLOSE); lv_obj_set_style_text_font(hx_ico, FONT_ICON, 0);
                    lv_obj_set_style_text_color(hx_ico, C_MUTED, 0); lv_obj_center(hx_ico);
                    lv_obj_add_event_cb(btn_hx, [](lv_event_t* e){
                        lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
                    }, LV_EVENT_CLICKED, hold_ov);

                    lv_obj_t* hold_title = lv_label_create(hold_ov);
                    lv_label_set_text(hold_title, t(K_MANTEN_PRESIONADO));
                    lv_obj_set_style_text_color(hold_title, C_TEXT, 0);
                    lv_obj_set_style_text_font(hold_title, FONT_HDR, 0);
                    lv_obj_align(hold_title, LV_ALIGN_TOP_MID, 0, 12);

                    lv_obj_t* hold_sub = lv_label_create(hold_ov);
                    lv_label_set_text(hold_sub, t(K_SUELTA_CANCELAR));
                    lv_obj_set_style_text_color(hold_sub, C_MUTED, 0);
                    lv_obj_set_style_text_font(hold_sub, FONT_SM, 0);
                    lv_obj_align(hold_sub, LV_ALIGN_TOP_MID, 0, 40);

                    // Barra de progreso
                    lv_obj_t* bar_bg = lv_obj_create(hold_ov);
                    lv_obj_set_size(bar_bg, UI_W - 48, 8); lv_obj_align(bar_bg, LV_ALIGN_CENTER, 0, -20);
                    lv_obj_set_style_bg_color(bar_bg, C_BORDER, 0); lv_obj_set_style_bg_opa(bar_bg, LV_OPA_COVER, 0);
                    lv_obj_set_style_border_width(bar_bg, 0, 0); lv_obj_set_style_radius(bar_bg, 4, 0);
                    lv_obj_set_style_pad_all(bar_bg, 0, 0); lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_SCROLLABLE);

                    lv_obj_t* bar_fill = lv_obj_create(bar_bg);
                    lv_obj_set_size(bar_fill, 0, 8); lv_obj_set_pos(bar_fill, 0, 0);
                    lv_obj_set_style_bg_color(bar_fill, C_DANGER, 0); lv_obj_set_style_bg_opa(bar_fill, LV_OPA_COVER, 0);
                    lv_obj_set_style_border_width(bar_fill, 0, 0); lv_obj_set_style_radius(bar_fill, 4, 0);
                    lv_obj_set_style_pad_all(bar_fill, 0, 0);

                    // Botón hold
                    lv_obj_t* btn_hold = lv_btn_create(hold_ov);
                    _press_fb(btn_hold);
                    lv_obj_set_size(btn_hold, UI_W - 48, 48);
                    lv_obj_align(btn_hold, LV_ALIGN_CENTER, 0, 30);
                    lv_obj_set_style_bg_color(btn_hold, C_DANGER, 0);
                    lv_obj_set_style_radius(btn_hold, 12, 0);
                    lv_obj_set_style_shadow_width(btn_hold, 0, 0);
                    lv_obj_set_style_border_width(btn_hold, 0, 0);
                    lv_obj_t* hold_lbl = lv_label_create(btn_hold);
                    lv_label_set_text(hold_lbl, t(K_RESTABLECER));
                    lv_obj_set_style_text_color(hold_lbl, lv_color_hex(0xFFFFFF), 0);
                    lv_obj_set_style_text_font(hold_lbl, FONT_MD, 0);
                    lv_obj_center(hold_lbl);

                    // Estado del hold
                    struct HoldCtx {
                        lv_obj_t* bar_fill;
                        lv_obj_t* hold_ov;
                        lv_obj_t* pin_ov;
                        uint32_t  press_start;
                        bool      pressing;
                        int       bar_w;
                    };
                    static HoldCtx hctx;
                    hctx = {bar_fill, hold_ov, pin_overlay, 0, false, UI_W - 48};

                    lv_obj_add_event_cb(btn_hold, [](lv_event_t* ev){
                        HoldCtx* h = (HoldCtx*)lv_event_get_user_data(ev);
                        h->press_start = lv_tick_get();
                        h->pressing = true;
                    }, LV_EVENT_PRESSED, &hctx);

                    lv_obj_add_event_cb(btn_hold, [](lv_event_t* ev){
                        HoldCtx* h = (HoldCtx*)lv_event_get_user_data(ev);
                        h->pressing = false;
                        // Resetear barra
                        if (h->bar_fill && lv_obj_is_valid(h->bar_fill))
                            lv_obj_set_width(h->bar_fill, 0);
                    }, LV_EVENT_RELEASED, &hctx);

                    lv_obj_add_event_cb(btn_hold, [](lv_event_t* ev){
                        HoldCtx* h = (HoldCtx*)lv_event_get_user_data(ev);
                        if (!h->pressing) return;
                        uint32_t elapsed = lv_tick_get() - h->press_start;
                        const uint32_t HOLD_MS = 5000;
                        int w = (int)((float)elapsed / HOLD_MS * h->bar_w);
                        if (w > h->bar_w) w = h->bar_w;
                        if (h->bar_fill && lv_obj_is_valid(h->bar_fill))
                            lv_obj_set_width(h->bar_fill, w);
                        if (elapsed >= HOLD_MS) {
                            h->pressing = false;
                            // ¡Ejecutar!
                            lv_obj_t* ov = h->hold_ov;
                            lv_obj_t* pov = h->pin_ov;
                            vault_factory_reset();
                            if (ov && lv_obj_is_valid(ov)) lv_obj_del(ov);
                            if (pov && lv_obj_is_valid(pov)) lv_obj_del(pov);
                            pin_screen_show();
                        }
                    }, LV_EVENT_PRESSING, &hctx);

                    // Cerrar overlay de confirmación
                    if (pin_overlay && lv_obj_is_valid(pin_overlay)) lv_obj_del(pin_overlay);
                });
                lv_obj_set_pos(r_kb, 0, UI_H - CKB_H);
                kb_show();
                kb_set_ta(r_ta);
            }, LV_EVENT_CLICKED, nullptr);
        });
    card_end();

    lv_obj_t* bottom_pad = lv_obj_create(body);
    lv_obj_set_size(bottom_pad, 1, 7);
    lv_obj_set_pos(bottom_pad, 0, y);
    lv_obj_set_style_bg_opa(bottom_pad, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_pad, 0, 0);
    lv_obj_set_style_pad_all(bottom_pad, 0, 0);
    lv_obj_clear_flag(bottom_pad, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bottom_pad, LV_OBJ_FLAG_CLICKABLE);

    // Restaurar la posición de scroll si venimos de un sub-ajuste (p. ej.
    // fecha/hora al final de la lista), para no volver siempre al principio.
    if (_settings_restore && _settings_scroll_y > 0) {
        lv_obj_update_layout(body); // asegurar que la altura scrollable ya está calculada
        lv_obj_scroll_to_y(body, _settings_scroll_y, LV_ANIM_OFF);
    }
    _settings_restore = false;

    if (animated) _load_screen(scr); else _load_screen_instant(scr);
}

// ── PANTALLA: Personalización (tema + color de acento) ─────────
static void _show_personalization() {
    kb_reset();
    _current_screen = SCREEN_SETTINGS; _kb_active = false;
    lv_obj_t* scr = _make_screen();

    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_back = lv_btn_create(hdr);
    _press_fb(btn_back);
    lv_obj_set_size(btn_back, 40, 44);
    lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back);
    lv_label_set_text(back_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(back_ico, C_TEXT, 0);
    lv_obj_center(back_ico);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){ _show_settings(true); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, t(K_PERSONALIZACION));
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    _divider(scr, 44);

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_W, UI_H - 44);
    lv_obj_set_pos(body, 0, 44);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);

    // Sección: Tema ────────────────────────────────────────────
    lv_obj_t* sec1 = lv_label_create(body);
    lv_label_set_text(sec1, t(K_TEMA));
    lv_obj_set_style_text_color(sec1, C_MUTED, 0);
    lv_obj_set_style_text_font(sec1, FONT_SM, 0);
    lv_obj_set_pos(sec1, 16, 12);

    VaultCustomTheme ct = vault_get_custom_theme();
    int card_w = (UI_W - 48 - 24) / 3; // 3 cards, 2 gaps de 12
    const char* theme_names[3] = { t(K_OSCURO), t(K_CLARO), t(K_PERSONALIZADO) };
    uint32_t theme_bg[3]   = { 0x1C1C1E, 0xF5F5F3, (uint32_t)lv_color_to32(hsv_to_lv_color(ct.bg_h, ct.bg_s, ct.base == 0 ? 16 : 94)) };
    uint32_t theme_card[3] = { 0x2C2C2E, 0xFFFFFF, (uint32_t)lv_color_to32(hsv_to_lv_color(ct.card_h, ct.card_s, ct.base == 0 ? 24 : 100)) };
    uint32_t theme_fg[3]   = { 0xFFFFFF, 0x000000, (uint32_t)(color_is_light(lv_color_hex(theme_bg[2])) ? 0x000000 : 0xFFFFFF) };
    for (int i = 0; i < 3; i++) {
        int x = 12 + i * (card_w + 12);
        lv_obj_t* card = lv_obj_create(body);
        lv_obj_set_size(card, card_w, 92);
        lv_obj_set_pos(card, x, 36);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(theme_bg[i]), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, (_current_theme == i) ? 2 : 0, 0);
        lv_obj_set_style_border_color(card, C_ACCENT, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        _press_fb(card);

        lv_obj_t* prev = lv_obj_create(card);
        lv_obj_remove_style_all(prev);
        lv_obj_set_size(prev, card_w - 16, 32);
        lv_obj_set_pos(prev, 8, 8);
        lv_obj_set_style_radius(prev, 8, 0);
        lv_obj_set_style_bg_color(prev, lv_color_hex(theme_card[i]), 0);
        lv_obj_set_style_bg_opa(prev, LV_OPA_COVER, 0);
        lv_obj_clear_flag(prev, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(card);
        lv_label_set_text(lbl, theme_names[i]);
        lv_obj_set_style_text_color(lbl, lv_color_hex(theme_fg[i]), 0);
        lv_obj_set_style_text_font(lbl, FONT_SM, 0);
        lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 48);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

        if (i == 2) {
            // Personalizado: SIEMPRE abre la pantalla avanzada (no aplica nada
            // por sí sola — el tema sólo cambia si ahí se confirma con el check).
            lv_obj_add_event_cb(card, [](lv_event_t* e){ _show_personalizacion_avanzada(); }, LV_EVENT_CLICKED, nullptr);
        } else {
            lv_obj_add_event_cb(card, [](lv_event_t* e){
                int idx = (int)(intptr_t)lv_event_get_user_data(e);
                _apply_theme(idx);
                vault_set_theme(idx);
                _show_personalization();
            }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
    }

    // Sección: Color de acento — sólo disponible con tema Claro/Oscuro.
    // En Personalizado el acento se define en la pantalla avanzada, no aquí.
    if (_current_theme != THEME_PERSONALIZADO) {
        _divider(body, 140);

        lv_obj_t* sec2 = lv_label_create(body);
        lv_label_set_text(sec2, t(K_COLOR_ACENTO));
        lv_obj_set_style_text_color(sec2, C_MUTED, 0);
        lv_obj_set_style_text_font(sec2, FONT_SM, 0);
        lv_obj_set_pos(sec2, 16, 152);

        lv_obj_t* acc_row = lv_obj_create(body);
        lv_obj_set_size(acc_row, UI_W, 44);
        lv_obj_set_pos(acc_row, 0, 174);
        lv_obj_set_style_bg_opa(acc_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(acc_row, 0, 0);
        lv_obj_set_style_pad_all(acc_row, 0, 0);
        lv_obj_set_style_radius(acc_row, 0, 0);
        lv_obj_clear_flag(acc_row, LV_OBJ_FLAG_SCROLLABLE);

        int cur_acc = vault_get_accent();
        lv_obj_t* sw = lv_obj_create(acc_row);
        lv_obj_set_size(sw, 24, 24);
        lv_obj_set_pos(sw, 16, 10);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
        if (cur_acc == ACCENT_AUTO) {
            lv_obj_set_style_bg_opa(sw, LV_OPA_TRANSP, 0);
            lv_obj_set_style_clip_corner(sw, true, 0);
            lv_obj_set_style_pad_all(sw, 0, 0);
            lv_obj_t* h1 = lv_obj_create(sw); lv_obj_remove_style_all(h1);
            lv_obj_set_size(h1, 12, 24); lv_obj_set_pos(h1, 0, 0);
            lv_obj_set_style_bg_color(h1, lv_color_hex(0x1C1C1E), 0); lv_obj_set_style_bg_opa(h1, LV_OPA_COVER, 0);
            lv_obj_clear_flag(h1, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t* h2 = lv_obj_create(sw); lv_obj_remove_style_all(h2);
            lv_obj_set_size(h2, 12, 24); lv_obj_set_pos(h2, 12, 0);
            lv_obj_set_style_bg_color(h2, lv_color_hex(0xE7EBF1), 0); lv_obj_set_style_bg_opa(h2, LV_OPA_COVER, 0);
            lv_obj_clear_flag(h2, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_set_style_bg_color(sw, C_ACCENT, 0);
            lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
        }
        lv_obj_set_style_border_width(sw, 0, 0);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* acc_lbl = lv_label_create(acc_row);
        const char* acc_names[ACCENT_COUNT] = {
            t(K_ACENTO_AZUL), t(K_ACENTO_VERDE), t(K_ACENTO_NARANJA),
            t(K_ACENTO_ROSA), t(K_ACENTO_MORADO), t(K_ACENTO_CELESTE)
        };
        lv_label_set_text(acc_lbl, cur_acc == ACCENT_AUTO ? t(K_SEGUN_TEMA) :
            (cur_acc >= 0 && cur_acc < ACCENT_COUNT ? acc_names[cur_acc] : acc_names[0]));
        lv_obj_set_style_text_color(acc_lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(acc_lbl, FONT_MD, 0);
        lv_obj_set_pos(acc_lbl, 52, 12);
        lv_obj_clear_flag(acc_lbl, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* chev = lv_label_create(acc_row);
        lv_label_set_text(chev, ICO_CHEVRON_RIGHT);
        lv_obj_set_style_text_font(chev, FONT_ICON, 0);
        lv_obj_set_style_text_color(chev, C_MUTED, 0);
        lv_obj_set_pos(chev, UI_W - 36, 10);
        lv_obj_clear_flag(chev, LV_OBJ_FLAG_CLICKABLE);

        _press_fb(acc_row);
        lv_obj_add_event_cb(acc_row, [](lv_event_t* e){
            lv_obj_t* scr_act = lv_scr_act();

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

            const int SH = 198;
            lv_obj_t* sheet = lv_obj_create(overlay);
            lv_obj_set_size(sheet, UI_W, SH);
            lv_obj_set_pos(sheet, 0, UI_H - SH);
            lv_obj_set_style_bg_color(sheet, C_BG, 0);
            lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(sheet, 0, 0);
            lv_obj_set_style_radius(sheet, 12, 0);
            lv_obj_set_style_pad_all(sheet, 0, 0);
            lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_OFF);
            lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* sh_title = lv_label_create(sheet);
            lv_label_set_text(sh_title, t(K_COLOR_ACENTO));
            lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
            lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
            lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);

            _divider(sheet, 40);

            int cur_acc2 = vault_get_accent();
            const int D = 44, GAP = 16, COLS = 4;
            int grid_w = COLS * D + (COLS - 1) * GAP;
            int x0 = (UI_W - grid_w) / 2;
            int y0 = 58;
            // 6 colores + "Según tema" = 7 círculos en grid de 4 columnas
            for (int i = 0; i < ACCENT_COUNT + 1; i++) {
                int col = i % COLS, row_i = i / COLS;
                int cx = x0 + col * (D + GAP);
                int cy = y0 + row_i * (D + GAP);
                bool is_auto = (i == ACCENT_COUNT);
                bool selected = is_auto ? (cur_acc2 == ACCENT_AUTO) : (cur_acc2 == i);

                lv_obj_t* sw2 = lv_obj_create(sheet);
                lv_obj_set_size(sw2, D, D);
                lv_obj_set_pos(sw2, cx, cy);
                lv_obj_set_style_radius(sw2, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_clip_corner(sw2, true, 0);
                lv_obj_set_style_pad_all(sw2, 0, 0);
                lv_obj_set_style_border_width(sw2, selected ? 3 : 0, 0);
                lv_obj_set_style_border_color(sw2, C_TEXT, 0);
                lv_obj_clear_flag(sw2, LV_OBJ_FLAG_SCROLLABLE);
                _press_fb(sw2);

                if (is_auto) {
                    lv_obj_set_style_bg_opa(sw2, LV_OPA_TRANSP, 0);
                    lv_obj_t* h1 = lv_obj_create(sw2); lv_obj_remove_style_all(h1);
                    lv_obj_set_size(h1, D / 2, D); lv_obj_set_pos(h1, 0, 0);
                    lv_obj_set_style_bg_color(h1, lv_color_hex(0x1C1C1E), 0); lv_obj_set_style_bg_opa(h1, LV_OPA_COVER, 0);
                    lv_obj_clear_flag(h1, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_t* h2 = lv_obj_create(sw2); lv_obj_remove_style_all(h2);
                    lv_obj_set_size(h2, D - D / 2, D); lv_obj_set_pos(h2, D / 2, 0);
                    lv_obj_set_style_bg_color(h2, lv_color_hex(0xE7EBF1), 0); lv_obj_set_style_bg_opa(h2, LV_OPA_COVER, 0);
                    lv_obj_clear_flag(h2, LV_OBJ_FLAG_CLICKABLE);
                } else {
                    lv_obj_set_style_bg_color(sw2, lv_color_hex(ACCENT_PALETTE[i]), 0);
                    lv_obj_set_style_bg_opa(sw2, LV_OPA_COVER, 0);
                }

                lv_obj_add_event_cb(sw2, [](lv_event_t* ev){
                    int idx = (int)(intptr_t)lv_event_get_user_data(ev);
                    lv_obj_t* row = (lv_obj_t*)lv_event_get_target(ev);
                    lv_obj_del(lv_obj_get_parent(lv_obj_get_parent(row))); // borra overlay completo
                    vault_set_accent(idx); // ACCENT_AUTO (-1) o índice 0..5
                    _apply_theme(_current_theme);
                    _show_personalization();
                }, LV_EVENT_CLICKED, (void*)(intptr_t)(is_auto ? ACCENT_AUTO : i));
            }

            lv_anim_t ash2;
            lv_anim_init(&ash2); lv_anim_set_var(&ash2, sheet);
            lv_anim_set_exec_cb(&ash2, (lv_anim_exec_xcb_t)lv_obj_set_y);
            lv_anim_set_values(&ash2, UI_H, UI_H - SH);
            lv_anim_set_time(&ash2, 220); lv_anim_set_path_cb(&ash2, lv_anim_path_ease_out);
            lv_anim_start(&ash2);
        }, LV_EVENT_CLICKED, nullptr);
    }

    _load_screen(scr);
}

// ── PANTALLA: Personalización avanzada (Fondo/Tarjeta/Acento) ──
//  Hue + Intensidad libres. Todo es borrador en RAM hasta tocar el
//  check (✓): ahí se persiste y el tema activo pasa a Personalizado.
//  Si se sale con back sin confirmar, no se guarda ni se aplica nada.
struct _PaHS { int h; int s; };
static int _pa_base   = 0;  // 0 = oscuro, 1 = claro (brillo base de fondo/tarjeta)
static int _pa_target  = 0; // 0 = fondo, 1 = tarjeta, 2 = acento
static _PaHS _pa_bg[2]   = { {210, 8}, {210, 6}  };
static _PaHS _pa_card[2] = { {210,14}, {210,10}  };
static _PaHS _pa_accent  = { 211, 96 };
static const int _PA_V_BG[2]   = { 16, 94  };
static const int _PA_V_CARD[2] = { 24, 100 };
static const int _PA_V_ACCENT  = 92;

#define PA_HUE_CV_W 208
#define PA_HUE_CV_H 14
static lv_color_t _pa_hue_cv_buf[PA_HUE_CV_W * PA_HUE_CV_H];
static lv_obj_t* _pa_hue_canvas  = nullptr;
static lv_obj_t* _pa_preview_bg  = nullptr;
static lv_obj_t* _pa_card_box    = nullptr;
static lv_obj_t* _pa_btn_box     = nullptr;
static lv_obj_t* _pa_btn_lbl     = nullptr;
static lv_obj_t* _pa_lbl_sample  = nullptr;
static lv_obj_t* _pa_hex_lbl     = nullptr;
static lv_obj_t* _pa_hue_slider  = nullptr;
static lv_obj_t* _pa_sat_slider  = nullptr;
static lv_obj_t* _pa_hue_val     = nullptr;
static lv_obj_t* _pa_sat_val     = nullptr;
static lv_obj_t* _pa_tab_bg      = nullptr;
static lv_obj_t* _pa_tab_card    = nullptr;
static lv_obj_t* _pa_tab_accent  = nullptr;
static lv_obj_t* _pa_theme_dark  = nullptr;
static lv_obj_t* _pa_theme_light = nullptr;
static lv_obj_t* _pa_scr         = nullptr;
static lv_obj_t* _pa_back_ico    = nullptr;
static lv_obj_t* _pa_title       = nullptr;
static lv_obj_t* _pa_hue_lbl_row = nullptr;
static lv_obj_t* _pa_sat_lbl_row = nullptr;

static void _pa_fill_hue_canvas() {
    for (int x = 0; x < PA_HUE_CV_W; x++) {
        int h = (x * 360) / PA_HUE_CV_W;
        lv_color_t c = hsv_to_lv_color(h, 100, 100);
        for (int y = 0; y < PA_HUE_CV_H; y++) lv_canvas_set_px(_pa_hue_canvas, x, y, c);
    }
}

static _PaHS* _pa_current_ref() {
    if (_pa_target == 0) return &_pa_bg[_pa_base];
    if (_pa_target == 1) return &_pa_card[_pa_base];
    return &_pa_accent;
}

static lv_color_t _pa_color_for(int which) { // 0=bg,1=card,2=accent
    if (which == 0) { _PaHS* r = &_pa_bg[_pa_base];   return hsv_to_lv_color(r->h, r->s, _PA_V_BG[_pa_base]); }
    if (which == 1) { _PaHS* r = &_pa_card[_pa_base]; return hsv_to_lv_color(r->h, r->s, _PA_V_CARD[_pa_base]); }
    return hsv_to_lv_color(_pa_accent.h, _pa_accent.s, _PA_V_ACCENT);
}

static void _pa_set_tab_style(lv_obj_t* tab, bool active) {
    lv_obj_set_style_bg_color(tab, active ? lv_color_hex(0x3A3A3D) : lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(tab, active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_t* lbl = lv_obj_get_child(tab, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, active ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x9A9A9E), 0);
}

static void _pa_refresh() {
    lv_color_t bg_c     = _pa_color_for(0);
    lv_color_t card_c   = _pa_color_for(1);
    lv_color_t accent_c = _pa_color_for(2);
    lv_color_t txt_c = color_is_light(bg_c) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);

    lv_obj_set_style_bg_color(_pa_scr, bg_c, 0);
    lv_obj_set_style_bg_color(_pa_preview_bg, bg_c, 0);
    lv_obj_set_style_bg_color(_pa_card_box, card_c, 0);
    lv_obj_set_style_bg_color(_pa_btn_box, accent_c, 0);
    lv_obj_set_style_text_color(_pa_lbl_sample, txt_c, 0);
    if (_pa_btn_lbl) lv_obj_set_style_text_color(_pa_btn_lbl, color_is_light(accent_c) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF), 0);
    if (_pa_back_ico)    lv_obj_set_style_text_color(_pa_back_ico, txt_c, 0);
    if (_pa_title)       lv_obj_set_style_text_color(_pa_title, txt_c, 0);
    if (_pa_hue_lbl_row) lv_obj_set_style_text_color(_pa_hue_lbl_row, txt_c, 0);
    if (_pa_sat_lbl_row) lv_obj_set_style_text_color(_pa_sat_lbl_row, txt_c, 0);
    if (_pa_hue_val)     lv_obj_set_style_text_color(_pa_hue_val, txt_c, 0);
    if (_pa_sat_val)     lv_obj_set_style_text_color(_pa_sat_val, txt_c, 0);
    if (_pa_hex_lbl)     lv_obj_set_style_text_color(_pa_hex_lbl, txt_c, 0);

    _PaHS* ref = _pa_current_ref();
    lv_slider_set_value(_pa_hue_slider, ref->h, LV_ANIM_OFF);
    lv_slider_set_value(_pa_sat_slider, ref->s, LV_ANIM_OFF);
    lv_label_set_text_fmt(_pa_hue_val, "%d°", ref->h);
    lv_label_set_text_fmt(_pa_sat_val, "%d%%", ref->s);

    lv_color_t active_c = (_pa_target == 0) ? bg_c : (_pa_target == 1) ? card_c : accent_c;
    uint32_t hexv = lv_color_to32(active_c) & 0xFFFFFF;
    lv_label_set_text_fmt(_pa_hex_lbl, "#%06X", (unsigned int)hexv);

    _pa_set_tab_style(_pa_tab_bg,     _pa_target == 0);
    _pa_set_tab_style(_pa_tab_card,   _pa_target == 1);
    _pa_set_tab_style(_pa_tab_accent, _pa_target == 2);
    _pa_set_tab_style(_pa_theme_dark,  _pa_base == 0);
    _pa_set_tab_style(_pa_theme_light, _pa_base == 1);
    lv_obj_set_style_opa(_pa_theme_dark,  (_pa_target == 2) ? LV_OPA_40 : LV_OPA_COVER, 0);
    lv_obj_set_style_opa(_pa_theme_light, (_pa_target == 2) ? LV_OPA_40 : LV_OPA_COVER, 0);
}

static void _pa_slider_cb(lv_event_t* e) {
    lv_obj_t* s = lv_event_get_target(e);
    _PaHS* ref = _pa_current_ref();
    if (s == _pa_hue_slider) ref->h = lv_slider_get_value(s);
    else                     ref->s = lv_slider_get_value(s);
    _pa_refresh();
}

static lv_obj_t* _pa_make_tab(lv_obj_t* parent, const char* text, int x, int w) {
    lv_obj_t* tab = lv_obj_create(parent);
    lv_obj_set_size(tab, w, 28);
    lv_obj_set_pos(tab, x, 0);
    lv_obj_set_style_radius(tab, 7, 0);
    lv_obj_set_style_border_width(tab, 0, 0);
    lv_obj_set_style_pad_all(tab, 0, 0);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl = lv_label_create(tab);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, FONT_SM, 0);
    lv_obj_center(lbl);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    return tab;
}

static void _show_personalizacion_avanzada() {
    kb_reset();
    _current_screen = SCREEN_SETTINGS; _kb_active = false;
    ui_statusbar_set_visible(false); // esta pantalla maneja su propio header, igual que lock_screen.h

    // Carga el borrador desde lo último guardado (o defaults si nunca se configuró)
    VaultCustomTheme ct = vault_get_custom_theme();
    _pa_base = ct.base;
    _pa_bg[_pa_base]   = { ct.bg_h,   ct.bg_s };
    _pa_card[_pa_base] = { ct.card_h, ct.card_s };
    _pa_accent         = { ct.acc_h,  ct.acc_s };
    _pa_target = 0;

    lv_obj_t* scr = lv_obj_create(NULL);
    _pa_scr = scr;
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header: back (descarta) + título + check (confirma)
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 36);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_back = lv_btn_create(hdr);
    lv_obj_set_size(btn_back, 36, 36);
    lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back);
    _pa_back_ico = back_ico;
    lv_label_set_text(back_ico, ICO_ARROW_BACK);
    lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_center(back_ico);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){ ui_statusbar_set_visible(true); _show_personalization(); }, LV_EVENT_CLICKED, nullptr); // descarta — no persiste nada

    lv_obj_t* btn_ok = lv_btn_create(hdr);
    lv_obj_set_size(btn_ok, 36, 36);
    lv_obj_set_pos(btn_ok, UI_W - 36, 0);
    lv_obj_set_style_bg_opa(btn_ok, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_ok, 0, 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_t* ok_ico = lv_label_create(btn_ok);
    lv_label_set_text(ok_ico, ICO_CHECK);
    lv_obj_set_style_text_font(ok_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(ok_ico, lv_color_hex(0x30D158), 0);
    lv_obj_center(ok_ico);
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
        VaultCustomTheme c;
        c.base = _pa_base;
        c.bg_h = _pa_bg[_pa_base].h;   c.bg_s = _pa_bg[_pa_base].s;
        c.card_h = _pa_card[_pa_base].h; c.card_s = _pa_card[_pa_base].s;
        c.acc_h = _pa_accent.h; c.acc_s = _pa_accent.s;
        vault_set_custom_theme(c);
        vault_set_theme(THEME_PERSONALIZADO);
        _apply_theme(THEME_PERSONALIZADO);
        ui_statusbar_set_visible(true);
        _show_personalization();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = lv_label_create(hdr);
    _pa_title = title;
    lv_label_set_text(title, t(K_PERSONALIZADO));
    lv_obj_set_style_text_font(title, FONT_SM, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Tabs de base (oscuro/claro) — no aplica al editar Acento
    lv_obj_t* theme_tabs = lv_obj_create(scr);
    lv_obj_set_size(theme_tabs, UI_W - 24, 28);
    lv_obj_set_pos(theme_tabs, 12, 40);
    lv_obj_set_style_bg_color(theme_tabs, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(theme_tabs, LV_OPA_10, 0);
    lv_obj_set_style_border_width(theme_tabs, 0, 0);
    lv_obj_set_style_radius(theme_tabs, 8, 0);
    lv_obj_set_style_pad_all(theme_tabs, 0, 0);
    lv_obj_clear_flag(theme_tabs, LV_OBJ_FLAG_SCROLLABLE);
    int half = (UI_W - 24) / 2;
    _pa_theme_dark  = _pa_make_tab(theme_tabs, t(K_OSCURO), 0, half);
    _pa_theme_light = _pa_make_tab(theme_tabs, t(K_CLARO), half, half);
    lv_obj_add_event_cb(_pa_theme_dark,  [](lv_event_t* e){ if (_pa_target != 2) { _pa_base = 0; _pa_refresh(); } }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(_pa_theme_light, [](lv_event_t* e){ if (_pa_target != 2) { _pa_base = 1; _pa_refresh(); } }, LV_EVENT_CLICKED, nullptr);

    // Tabs de target
    lv_obj_t* target_tabs = lv_obj_create(scr);
    lv_obj_set_size(target_tabs, UI_W - 24, 28);
    lv_obj_set_pos(target_tabs, 12, 74);
    lv_obj_set_style_bg_color(target_tabs, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(target_tabs, LV_OPA_10, 0);
    lv_obj_set_style_border_width(target_tabs, 0, 0);
    lv_obj_set_style_radius(target_tabs, 8, 0);
    lv_obj_set_style_pad_all(target_tabs, 0, 0);
    lv_obj_clear_flag(target_tabs, LV_OBJ_FLAG_SCROLLABLE);
    int third = (UI_W - 24) / 3;
    _pa_tab_bg     = _pa_make_tab(target_tabs, t(K_FONDO), 0, third);
    _pa_tab_card   = _pa_make_tab(target_tabs, t(K_TARJETA), third, third);
    _pa_tab_accent = _pa_make_tab(target_tabs, t(K_BOTON), third * 2, third);
    lv_obj_add_event_cb(_pa_tab_bg,     [](lv_event_t* e){ _pa_target = 0; _pa_refresh(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(_pa_tab_card,   [](lv_event_t* e){ _pa_target = 1; _pa_refresh(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(_pa_tab_accent, [](lv_event_t* e){ _pa_target = 2; _pa_refresh(); }, LV_EVENT_CLICKED, nullptr);

    // Preview en vivo
    _pa_preview_bg = lv_obj_create(scr);
    lv_obj_set_size(_pa_preview_bg, UI_W - 24, 76);
    lv_obj_set_pos(_pa_preview_bg, 12, 108);
    lv_obj_set_style_radius(_pa_preview_bg, 12, 0);
    lv_obj_set_style_border_width(_pa_preview_bg, 1, 0);
    lv_obj_set_style_border_color(_pa_preview_bg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(_pa_preview_bg, LV_OPA_10, 0);
    lv_obj_set_style_pad_all(_pa_preview_bg, 10, 0);
    lv_obj_clear_flag(_pa_preview_bg, LV_OBJ_FLAG_SCROLLABLE);

    _pa_lbl_sample = lv_label_create(_pa_preview_bg);
    lv_label_set_text(_pa_lbl_sample, t(K_TEXTO_MUESTRA));
    lv_obj_set_style_text_font(_pa_lbl_sample, FONT_SM, 0);
    lv_obj_set_pos(_pa_lbl_sample, 0, 0);

    int inner_w = (UI_W - 24) - 20;
    int gap = 8;
    int card_w2 = (int)((inner_w - gap) * 0.55f);
    int btn_w   = inner_w - gap - card_w2;

    _pa_card_box = lv_obj_create(_pa_preview_bg);
    lv_obj_set_size(_pa_card_box, card_w2, 26);
    lv_obj_set_pos(_pa_card_box, 0, 24);
    lv_obj_set_style_radius(_pa_card_box, 7, 0);
    lv_obj_set_style_border_width(_pa_card_box, 0, 0);
    lv_obj_clear_flag(_pa_card_box, LV_OBJ_FLAG_SCROLLABLE);

    _pa_btn_box = lv_obj_create(_pa_preview_bg);
    lv_obj_set_size(_pa_btn_box, btn_w, 26);
    lv_obj_set_pos(_pa_btn_box, card_w2 + gap, 24);
    lv_obj_set_style_radius(_pa_btn_box, 7, 0);
    lv_obj_set_style_border_width(_pa_btn_box, 0, 0);
    lv_obj_clear_flag(_pa_btn_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* btn_lbl = lv_label_create(_pa_btn_box);
    _pa_btn_lbl = btn_lbl;
    lv_label_set_text(btn_lbl, t(K_GUARDAR));
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(btn_lbl, FONT_SM, 0);
    lv_obj_center(btn_lbl);

    // Sliders
    lv_obj_t* hue_lbl_row = lv_label_create(scr);
    _pa_hue_lbl_row = hue_lbl_row;
    lv_label_set_text(hue_lbl_row, t(K_COLOR));
    lv_obj_set_style_text_font(hue_lbl_row, FONT_SM, 0);
    lv_obj_set_pos(hue_lbl_row, 16, 198);
    _pa_hue_val = lv_label_create(scr);
    lv_obj_set_style_text_font(_pa_hue_val, FONT_SM, 0);
    lv_obj_align(_pa_hue_val, LV_ALIGN_TOP_RIGHT, -16, 198);

    _pa_hue_canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(_pa_hue_canvas, _pa_hue_cv_buf, PA_HUE_CV_W, PA_HUE_CV_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_pa_hue_canvas, 16, 216);
    lv_obj_set_style_radius(_pa_hue_canvas, 7, 0);
    lv_obj_set_style_clip_corner(_pa_hue_canvas, true, 0);
    _pa_fill_hue_canvas();

    _pa_hue_slider = lv_slider_create(scr);
    lv_obj_set_size(_pa_hue_slider, UI_W - 32, 14);
    lv_obj_set_pos(_pa_hue_slider, 16, 216);
    lv_slider_set_range(_pa_hue_slider, 0, 360);
    lv_obj_set_style_bg_opa(_pa_hue_slider, LV_OPA_TRANSP, LV_PART_MAIN); // el degradado real lo pinta el canvas de atrás
    lv_obj_set_style_bg_color(_pa_hue_slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(_pa_hue_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_event_cb(_pa_hue_slider, _pa_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* sat_lbl_row = lv_label_create(scr);
    _pa_sat_lbl_row = sat_lbl_row;
    lv_label_set_text(sat_lbl_row, t(K_INTENSIDAD));
    lv_obj_set_style_text_font(sat_lbl_row, FONT_SM, 0);
    lv_obj_set_pos(sat_lbl_row, 16, 244);
    _pa_sat_val = lv_label_create(scr);
    lv_obj_set_style_text_font(_pa_sat_val, FONT_SM, 0);
    lv_obj_align(_pa_sat_val, LV_ALIGN_TOP_RIGHT, -16, 244);

    _pa_sat_slider = lv_slider_create(scr);
    lv_obj_set_size(_pa_sat_slider, UI_W - 32, 14);
    lv_obj_set_pos(_pa_sat_slider, 16, 262);
    lv_slider_set_range(_pa_sat_slider, 0, 100);
    lv_obj_set_style_bg_color(_pa_sat_slider, lv_color_hex(0x555558), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_pa_sat_slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
    lv_obj_add_event_cb(_pa_sat_slider, _pa_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    _pa_hex_lbl = lv_label_create(scr);
    lv_label_set_text(_pa_hex_lbl, "#000000");
    lv_obj_set_style_text_color(_pa_hex_lbl, lv_color_hex(0x9A9A9E), 0);
    lv_obj_set_style_text_font(_pa_hex_lbl, FONT_SM, 0);
    lv_obj_align(_pa_hex_lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

    _pa_refresh();
    lv_scr_load(scr);
}

// ── PANTALLAS: Fecha / Hora (misma estética que onboarding) ──

#ifndef OB_YEAR_MIN
#define OB_YEAR_MIN 2026
#define OB_YEAR_MAX 2100
#endif

static lv_obj_t* _st_roller[3];
static bool      _st_time_24h = true;

static void _st_build_num_options(char* buf, size_t buf_len, int from, int to, int width) {
    size_t pos = 0;
    for (int v = from; v <= to; v++) {
        size_t need = (size_t)width + 2;
        if (pos + need > buf_len) break;
        int n = snprintf(buf + pos, buf_len - pos, width == 4 ? "%04d" : "%02d", v);
        pos += n;
        if (v < to) { buf[pos++] = '\n'; }
    }
    buf[pos] = '\0';
}

static lv_obj_t* _st_make_roller(lv_obj_t* scr, int x, int y, int w, int h,
                                   const char* options, int sel, lv_roller_mode_t mode) {
    lv_obj_t* r = lv_roller_create(scr);
    lv_roller_set_options(r, options, mode);
    lv_roller_set_visible_row_count(r, 3);
    lv_obj_set_size(r, w, h);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_style_bg_color(r, C_CARD, LV_PART_MAIN);
    lv_obj_set_style_text_color(r, C_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(r, FONT_MD, LV_PART_MAIN);
    lv_obj_set_style_border_width(r, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(r, C_ACCENT, LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);
    lv_obj_set_style_text_font(r, FONT_HDR, LV_PART_SELECTED);
    lv_roller_set_selected(r, sel, LV_ANIM_OFF);
    return r;
}

static void _show_date_settings() {
    _current_screen = SCREEN_SETTINGS; _kb_active = false;
    lv_obj_t* scr = _make_screen();

    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bb = lv_obj_create(hdr);
    lv_obj_add_flag(bb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
    lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(bb, 0, 0);
    lv_obj_set_style_pad_all(bb, 0, 0); lv_obj_clear_flag(bb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bb_ico = lv_label_create(bb);
    lv_label_set_text(bb_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(bb_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(bb_ico, C_TEXT, 0); lv_obj_center(bb_ico);
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_settings(true); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, t(K_FECHA));
    lv_obj_set_style_text_font(title, FONT_HDR, 0);
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    int cur_y = rtc_get_year(), cur_m = rtc_get_month(), cur_d = rtc_get_day();
    int rw = 70, gap = 8, rh = 96;
    int total_w = 3 * rw + 2 * gap;
    int x0 = (UI_W - total_w) / 2;
    int y0 = 110;

    static char year_opts[420], month_opts[64], day_opts[100];
    _st_build_num_options(year_opts, sizeof(year_opts), OB_YEAR_MIN, OB_YEAR_MAX, 4);
    _st_build_num_options(month_opts, sizeof(month_opts), 1, 12, 2);
    _st_build_num_options(day_opts, sizeof(day_opts), 1, 31, 2);

    int sel_y = (cur_y < OB_YEAR_MIN) ? 0 : (cur_y > OB_YEAR_MAX ? OB_YEAR_MAX - OB_YEAR_MIN : cur_y - OB_YEAR_MIN);
    _st_roller[0] = _st_make_roller(scr, x0,               y0, rw, rh, year_opts,  sel_y,      LV_ROLLER_MODE_NORMAL);
    _st_roller[1] = _st_make_roller(scr, x0 + rw + gap,    y0, rw, rh, month_opts, cur_m - 1,  LV_ROLLER_MODE_INFINITE);
    _st_roller[2] = _st_make_roller(scr, x0 + 2*(rw+gap),  y0, rw, rh, day_opts,   cur_d - 1,  LV_ROLLER_MODE_INFINITE);

    // Botón Guardar
    lv_obj_t* btn = lv_obj_create(scr);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn, UI_W - 32, 44); lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(btn, C_ACCENT, 0); lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0); lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_t* btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, t(K_GUARDAR));
    lv_obj_set_style_text_font(btn_lbl, FONT_MD, 0);
    lv_obj_set_style_text_color(btn_lbl, C_ACCENT_TEXT, 0);
    lv_obj_center(btn_lbl);
    lv_obj_add_event_cb(btn, [](lv_event_t* e){
        int y = OB_YEAR_MIN + lv_roller_get_selected(_st_roller[0]);
        int m = 1 + lv_roller_get_selected(_st_roller[1]);
        int d = 1 + lv_roller_get_selected(_st_roller[2]);
        rtc_set(y, m, d, rtc_get_hour(), rtc_get_minute(), rtc_get_second());
        _show_settings(true);
    }, LV_EVENT_CLICKED, nullptr);

    _load_screen(scr);
}

static void _show_time_settings() {
    _st_time_24h = vault_get_time_24h();
    _current_screen = SCREEN_SETTINGS; _kb_active = false;
    lv_obj_t* scr = _make_screen();

    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bb = lv_obj_create(hdr);
    lv_obj_add_flag(bb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
    lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(bb, 0, 0);
    lv_obj_set_style_pad_all(bb, 0, 0); lv_obj_clear_flag(bb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bb_ico = lv_label_create(bb);
    lv_label_set_text(bb_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(bb_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(bb_ico, C_TEXT, 0); lv_obj_center(bb_ico);
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_settings(true); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, t(K_HORA));
    lv_obj_set_style_text_font(title, FONT_HDR, 0);
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Selector 24h/12h
    int seg_w = 70, seg_h = 28, seg_gap = 4;
    int seg_x0 = (UI_W - (2 * seg_w + seg_gap)) / 2;
    const char* seg_labels[2] = {t(K_24HORAS), t(K_12HORAS)};
    for (int i = 0; i < 2; i++) {
        bool sel = (i == 0) == _st_time_24h;
        lv_obj_t* b = lv_obj_create(scr);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(b, seg_w, seg_h);
        lv_obj_set_pos(b, seg_x0 + i * (seg_w + seg_gap), 56);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_bg_color(b, sel ? C_ACCENT : C_CARD, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, seg_labels[i]);
        lv_obj_set_style_text_color(l, sel ? lv_color_hex(0xFFFFFF) : C_TEXT, 0);
        lv_obj_set_style_text_font(l, FONT_SM, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, [](lv_event_t* ev){
            bool to24 = (bool)(intptr_t)lv_event_get_user_data(ev);
            if (to24 == _st_time_24h) return;
            _st_time_24h = to24;
            vault_set_time_24h(to24);
            _show_time_settings();
        }, LV_EVENT_CLICKED, (void*)(intptr_t)(i == 0));
    }

    int cur_h = rtc_get_hour(), cur_min = rtc_get_minute();
    int y0 = 100, rh = 96;

    if (_st_time_24h) {
        int rw = 80, gap = 16;
        int x0 = (UI_W - (2 * rw + gap)) / 2;
        static char hour_opts[80], min_opts[200];
        _st_build_num_options(hour_opts, sizeof(hour_opts), 0, 23, 2);
        _st_build_num_options(min_opts, sizeof(min_opts), 0, 59, 2);
        _st_roller[0] = _st_make_roller(scr, x0,           y0, rw, rh, hour_opts, cur_h,   LV_ROLLER_MODE_INFINITE);
        _st_roller[1] = _st_make_roller(scr, x0 + rw + gap,y0, rw, rh, min_opts,  cur_min, LV_ROLLER_MODE_INFINITE);
    } else {
        int rw = 56, gap = 8;
        int x0 = (UI_W - (3 * rw + 2 * gap)) / 2;
        int h12 = cur_h % 12; if (h12 == 0) h12 = 12;
        bool pm = (cur_h >= 12);
        static char hour12_opts[64], min_opts[200];
        static const char ampm_opts[] = "AM\nPM";
        _st_build_num_options(hour12_opts, sizeof(hour12_opts), 1, 12, 2);
        _st_build_num_options(min_opts, sizeof(min_opts), 0, 59, 2);
        _st_roller[0] = _st_make_roller(scr, x0,              y0, rw, rh, hour12_opts, h12 - 1,    LV_ROLLER_MODE_INFINITE);
        _st_roller[1] = _st_make_roller(scr, x0 + rw + gap,   y0, rw, rh, min_opts,   cur_min,    LV_ROLLER_MODE_INFINITE);
        _st_roller[2] = _st_make_roller(scr, x0 + 2*(rw+gap), y0, rw, rh, ampm_opts,  pm ? 1 : 0, LV_ROLLER_MODE_NORMAL);
    }

    // Botón Guardar
    lv_obj_t* btn = lv_obj_create(scr);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn, UI_W - 32, 44); lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(btn, C_ACCENT, 0); lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0); lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_t* btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, t(K_GUARDAR));
    lv_obj_set_style_text_font(btn_lbl, FONT_MD, 0);
    lv_obj_set_style_text_color(btn_lbl, C_ACCENT_TEXT, 0);
    lv_obj_center(btn_lbl);
    lv_obj_add_event_cb(btn, [](lv_event_t* e){
        int h, m;
        if (_st_time_24h) {
            h = lv_roller_get_selected(_st_roller[0]);
            m = lv_roller_get_selected(_st_roller[1]);
        } else {
            int h12 = 1 + lv_roller_get_selected(_st_roller[0]);
            m = lv_roller_get_selected(_st_roller[1]);
            bool pm = lv_roller_get_selected(_st_roller[2]) == 1;
            h = h12 % 12; if (pm) h += 12;
        }
        rtc_set(rtc_get_year(), rtc_get_month(), rtc_get_day(), h, m, 0);
        _show_settings(true);
    }, LV_EVENT_CLICKED, nullptr);

    _load_screen(scr);
}

// ── PANTALLA: Información ─────────────────────────────────────
static void _show_info() {
    _current_screen = SCREEN_INFO; _kb_active = false;
    lv_obj_t* scr = _make_screen();
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bb = lv_btn_create(hdr); lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
    lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0); lv_obj_set_style_shadow_width(bb, 0, 0); lv_obj_set_style_border_width(bb, 0, 0);
    lv_obj_t* bi = lv_label_create(bb); lv_label_set_text(bi, ICO_ARROW_BACK); lv_obj_set_style_text_font(bi, FONT_ICON, 0);
    lv_obj_set_style_text_color(bi, C_TEXT, 0); lv_obj_center(bi);
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_settings(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* tt = lv_label_create(hdr); lv_label_set_text(tt, t(K_INFORMACION));
    lv_obj_set_style_text_color(tt, C_TEXT, 0); lv_obj_set_style_text_font(tt, FONT_LG, 0); lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);
    _divider(scr, 44);

    int px = 16, y = 58, fw = UI_W - 32;
    auto info_row = [&](const char* label, const char* value) {
        lv_obj_t* lbl = lv_label_create(scr); lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, C_MUTED, 0); lv_obj_set_style_text_font(lbl, FONT_SM, 0); lv_obj_set_pos(lbl, px, y);
        lv_obj_t* val = lv_label_create(scr); lv_label_set_text(val, value);
        lv_obj_set_style_text_color(val, C_TEXT, 0); lv_obj_set_style_text_font(val, FONT_MD, 0); lv_obj_set_pos(val, px, y + 16);
        lv_obj_t* div = lv_obj_create(scr); lv_obj_set_size(div, fw, 1); lv_obj_set_pos(div, px, y + 36);
        lv_obj_set_style_bg_color(div, C_BORDER, 0); lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0); lv_obj_set_style_border_width(div, 0, 0); lv_obj_set_style_pad_all(div, 0, 0);
        y += 46;
    };

    info_row(t(K_VERSION_FIRMWARE), "v1.0.0 (2026)");
    int total = vault_count();
    char es[32]; snprintf(es, sizeof(es), t(K_FMT_N_ENTRADAS), total); info_row(t(K_ENTRADAS_GUARDADAS), es);
    char sp[32]; snprintf(sp, sizeof(sp), t(K_FMT_N_DE_N_ENTRADAS), total, VAULTFS_SLOT_COUNT); info_row(t(K_ESPACIO_USADO), sp);
    info_row(t(K_CIFRADO), "AES-256-CBC + SHA-256");
    _load_screen(scr);
}

// ── PANTALLA: Aviso de privacidad ─────────────────────────────
static void _show_privacy() {
    _current_screen = SCREEN_PRIVACY; _kb_active = false;
    lv_obj_t* scr = _make_screen();
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bb = lv_btn_create(hdr); lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
    lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0); lv_obj_set_style_shadow_width(bb, 0, 0); lv_obj_set_style_border_width(bb, 0, 0);
    lv_obj_t* bi = lv_label_create(bb); lv_label_set_text(bi, ICO_ARROW_BACK); lv_obj_set_style_text_font(bi, FONT_ICON, 0);
    lv_obj_set_style_text_color(bi, C_TEXT, 0); lv_obj_center(bi);
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_settings(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* tt = lv_label_create(hdr); lv_label_set_text(tt, t(K_PRIVACIDAD));
    lv_obj_set_style_text_color(tt, C_TEXT, 0); lv_obj_set_style_text_font(tt, FONT_LG, 0); lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);
    _divider(scr, 44);

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_W, UI_H - 44); lv_obj_set_pos(body, 0, 44);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0); lv_obj_set_style_pad_top(body, 14, 0); lv_obj_set_style_pad_bottom(body, 20, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN); lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);

    // Fuente unica compartida con el asistente inicial.
    lv_obj_t* notice = lv_label_create(body);
    lv_label_set_text(notice, t(K_LEGAL_TEXTO));
    lv_obj_set_style_text_color(notice, C_TEXT, 0);
    lv_obj_set_style_text_font(notice, FONT_SM, 0);
    lv_label_set_long_mode(notice, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(notice, UI_W - 32);

    lv_obj_t* note = lv_label_create(body);
    lv_label_set_text(note, t(K_AVISO_COMPLETO));
    lv_obj_set_style_text_color(note, C_MUTED, 0); lv_obj_set_style_text_font(note, FONT_SM, 0);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP); lv_obj_set_width(note, UI_W - 32);
    lv_obj_set_style_pad_top(note, 8, 0);
    _load_screen(scr);
}

// ── PANTALLA: Modo perdido (mano muerta) — explicación + activación ──
static const int DM_DAY_OPTS[6] = {7, 14, 30, 60, 90, 180};
static bool      _dm_checked = false;
static int       _dm_sel_days = 30;
static lv_obj_t* _dm_chk_box = nullptr;
static lv_obj_t* _dm_chk_ico = nullptr;
static lv_obj_t* _dm_btn_activate = nullptr;
static lv_obj_t* _dm_days_btns[6];

static void _dm_refresh_days_ui() {
    for (int i = 0; i < 6; i++) {
        if (!_dm_days_btns[i] || !lv_obj_is_valid(_dm_days_btns[i])) continue;
        bool sel = (DM_DAY_OPTS[i] == _dm_sel_days);
        lv_obj_set_style_bg_color(_dm_days_btns[i], sel ? C_ACCENT : C_CARD, 0);
        lv_obj_t* lbl = lv_obj_get_child(_dm_days_btns[i], 0);
        if (lbl) lv_obj_set_style_text_color(lbl, sel ? C_ACCENT_TEXT : C_TEXT, 0);
    }
}

static void _show_deadman_info() {
    _current_screen = SCREEN_PRIVACY; _kb_active = false;
    _dm_checked = false;
    _dm_sel_days = 30;

    lv_obj_t* scr = _make_screen();
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bb = lv_btn_create(hdr); lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
    lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0); lv_obj_set_style_shadow_width(bb, 0, 0); lv_obj_set_style_border_width(bb, 0, 0);
    lv_obj_t* bi = lv_label_create(bb); lv_label_set_text(bi, ICO_ARROW_BACK); lv_obj_set_style_text_font(bi, FONT_ICON, 0);
    lv_obj_set_style_text_color(bi, C_TEXT, 0); lv_obj_center(bi);
    _press_fb(bb);
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_settings(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* tt = lv_label_create(hdr); lv_label_set_text(tt, t(K_MODO_PERDIDO));
    lv_obj_set_style_text_color(tt, C_TEXT, 0); lv_obj_set_style_text_font(tt, FONT_LG, 0); lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);
    _divider(scr, 44);

    // Cuerpo scrolleable — deja espacio abajo para checkbox+botón fijos
    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_W, (UI_H / 2) - 44); lv_obj_set_pos(body, 0, 44);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0); lv_obj_set_style_pad_top(body, 14, 0); lv_obj_set_style_pad_bottom(body, 14, 0);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* h2 = lv_label_create(body);
    lv_label_set_text(h2, t(K_MP_TITULO));
    lv_obj_set_style_text_color(h2, C_TEXT, 0);
    lv_obj_set_style_text_font(h2, FONT_HDR, 0);
    lv_label_set_long_mode(h2, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h2, UI_W - 32);
    lv_obj_set_pos(h2, 0, 0);

    lv_obj_t* body_txt = lv_label_create(body);
    lv_label_set_text(body_txt, t(K_MP_BODY));
    lv_obj_set_style_text_color(body_txt, C_MUTED, 0);
    lv_obj_set_style_text_font(body_txt, FONT_SM, 0);
    lv_label_set_long_mode(body_txt, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body_txt, UI_W - 32);
    lv_obj_set_pos(body_txt, 0, 34);

    _divider(scr, UI_H / 2);

    // Fila fija inferior: selector de días
    lv_obj_t* dlbl = lv_label_create(scr);
    lv_label_set_text(dlbl, t(K_MP_DIAS));
    lv_obj_set_style_text_color(dlbl, C_TEXT, 0);
    lv_obj_set_style_text_font(dlbl, FONT_MD, 0);
    lv_obj_set_pos(dlbl, 16, UI_H / 2 + 6);

    int bw = (UI_W - 32 - 5 * 6) / 6;
    for (int i = 0; i < 6; i++) {
        lv_obj_t* db = lv_btn_create(scr);
        lv_obj_set_size(db, bw, 28);
        lv_obj_set_pos(db, 16 + i * (bw + 6), UI_H / 2 + 34);
        lv_obj_set_style_radius(db, 7, 0);
        lv_obj_set_style_shadow_width(db, 0, 0); lv_obj_set_style_border_width(db, 0, 0);
        _press_fb(db);
        lv_obj_t* dl = lv_label_create(db);
        char buf[8]; snprintf(buf, sizeof(buf), "%d", DM_DAY_OPTS[i]);
        lv_label_set_text(dl, buf);
        lv_obj_set_style_text_font(dl, FONT_SM, 0);
        lv_obj_center(dl);
        lv_obj_clear_flag(dl, LV_OBJ_FLAG_CLICKABLE);
        _dm_days_btns[i] = db;
        lv_obj_add_event_cb(db, [](lv_event_t* ev){
            int idx = (int)(intptr_t)lv_event_get_user_data(ev);
            _dm_sel_days = DM_DAY_OPTS[idx];
            _dm_refresh_days_ui();
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    _dm_refresh_days_ui();

    // Checkbox "he leído y entiendo" — fila clickeable grande (hitbox 40px)
    lv_obj_t* chk_row = lv_obj_create(scr);
    lv_obj_set_size(chk_row, UI_W - 32, 48);
    lv_obj_set_pos(chk_row, 16, UI_H / 2 + 64);
    lv_obj_set_style_bg_opa(chk_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chk_row, 0, 0);
    lv_obj_set_style_pad_all(chk_row, 0, 0);
    lv_obj_clear_flag(chk_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chk_row, LV_OBJ_FLAG_CLICKABLE);

    _dm_chk_box = lv_obj_create(chk_row);
    lv_obj_set_size(_dm_chk_box, 26, 26);
    lv_obj_set_pos(_dm_chk_box, 0, 11);
    lv_obj_set_style_radius(_dm_chk_box, 6, 0);
    lv_obj_set_style_bg_color(_dm_chk_box, C_BG, 0);
    lv_obj_set_style_bg_opa(_dm_chk_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_dm_chk_box, C_MUTED, 0);
    lv_obj_set_style_border_width(_dm_chk_box, 2, 0);
    lv_obj_set_style_pad_all(_dm_chk_box, 0, 0);
    lv_obj_clear_flag(_dm_chk_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_dm_chk_box, LV_OBJ_FLAG_CLICKABLE);

    _dm_chk_ico = lv_label_create(_dm_chk_box);
    lv_label_set_text(_dm_chk_ico, ICO_CHECK);
    lv_obj_set_style_text_font(_dm_chk_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(_dm_chk_ico, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(_dm_chk_ico);
    lv_obj_add_flag(_dm_chk_ico, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* chk_lbl = lv_label_create(chk_row);
    lv_label_set_text(chk_lbl, t(K_MP_HE_LEIDO));
    lv_obj_set_style_text_font(chk_lbl, FONT_MD, 0);
    lv_obj_set_style_text_color(chk_lbl, C_TEXT, 0);
    lv_label_set_long_mode(chk_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(chk_lbl, UI_W - 32 - 38);
    lv_obj_set_pos(chk_lbl, 38, 12);
    lv_obj_clear_flag(chk_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(chk_row, [](lv_event_t* ev){
        _dm_checked = !_dm_checked;
        lv_obj_set_style_bg_color(_dm_chk_box, _dm_checked ? C_ACCENT : C_BG, 0);
        lv_obj_set_style_border_color(_dm_chk_box, _dm_checked ? C_ACCENT : C_MUTED, 0);
        if (_dm_checked) lv_obj_clear_flag(_dm_chk_ico, LV_OBJ_FLAG_HIDDEN);
        else             lv_obj_add_flag(_dm_chk_ico, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(_dm_btn_activate, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(_dm_btn_activate, _dm_checked ? C_ACCENT : C_MUTED, 0);
    }, LV_EVENT_CLICKED, nullptr);

    // Botón activar — deshabilitado visualmente hasta marcar checkbox
    _dm_btn_activate = lv_btn_create(scr);
    lv_obj_set_size(_dm_btn_activate, UI_W - 32, 40);
    lv_obj_set_pos(_dm_btn_activate, 16, UI_H / 2 + 116);
    lv_obj_set_style_bg_color(_dm_btn_activate, C_MUTED, 0);
    lv_obj_set_style_radius(_dm_btn_activate, 10, 0);
    lv_obj_set_style_shadow_width(_dm_btn_activate, 0, 0);
    lv_obj_set_style_border_width(_dm_btn_activate, 0, 0);
    _press_fb(_dm_btn_activate);
    lv_obj_t* act_lbl = lv_label_create(_dm_btn_activate);
    lv_label_set_text(act_lbl, t(K_MP_ACTIVAR));
    lv_obj_set_style_text_color(act_lbl, C_ACCENT_TEXT, 0);
    lv_obj_set_style_text_font(act_lbl, FONT_MD, 0);
    lv_obj_center(act_lbl);
    lv_obj_add_event_cb(_dm_btn_activate, [](lv_event_t* ev){
        if (!_dm_checked) return;
        vault_set_deadman_days(_dm_sel_days);
        _show_settings();
    }, LV_EVENT_CLICKED, nullptr);

    _load_screen(scr);
}

// ── SHEET: Modo perdido ya activo — cambiar plazo / desactivar ──
static void _show_deadman_settings_sheet() {
    lv_obj_t* ov = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ov, UI_W, UI_H); lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    _fade_overlay(ov, LV_OPA_50);
    lv_obj_set_style_border_width(ov, 0, 0); lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ov, [](lv_event_t* ev){
        lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
    }, LV_EVENT_CLICKED, ov);

    const int SH = 250;
    lv_obj_t* sheet = lv_obj_create(ov);
    lv_obj_set_size(sheet, UI_W, SH); lv_obj_set_pos(sheet, 0, UI_H - SH);
    lv_obj_set_style_bg_color(sheet, C_BG, 0); lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sheet, 0, 0); lv_obj_set_style_radius(sheet, 12, 0);
    lv_obj_set_style_pad_all(sheet, 0, 0);
    lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* sh_title = lv_label_create(sheet);
    lv_label_set_text(sh_title, t(K_MP_AJUSTES));
    lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
    lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
    lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);
    _divider(sheet, 40);

    lv_obj_t* lbl_d = lv_label_create(sheet);
    lv_label_set_text(lbl_d, t(K_MP_DIAS));
    lv_obj_set_style_text_color(lbl_d, C_MUTED, 0);
    lv_obj_set_style_text_font(lbl_d, FONT_SM, 0);
    lv_obj_set_pos(lbl_d, 16, 50);

    int cur = vault_get_deadman_days();
    int bw = (UI_W - 32 - 5 * 6) / 6;
    for (int i = 0; i < 6; i++) {
        lv_obj_t* db = lv_btn_create(sheet);
        lv_obj_set_size(db, bw, 30);
        lv_obj_set_pos(db, 16 + i * (bw + 6), 70);
        bool sel = (DM_DAY_OPTS[i] == cur);
        lv_obj_set_style_bg_color(db, sel ? C_ACCENT : C_CARD, 0);
        lv_obj_set_style_radius(db, 7, 0);
        lv_obj_set_style_shadow_width(db, 0, 0); lv_obj_set_style_border_width(db, 0, 0);
        _press_fb(db);
        lv_obj_t* dl = lv_label_create(db);
        char buf[8]; snprintf(buf, sizeof(buf), "%d", DM_DAY_OPTS[i]);
        lv_label_set_text(dl, buf);
        lv_obj_set_style_text_color(dl, sel ? C_ACCENT_TEXT : C_TEXT, 0);
        lv_obj_set_style_text_font(dl, FONT_SM, 0);
        lv_obj_center(dl);
        lv_obj_clear_flag(dl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(db, [](lv_event_t* ev){
            int idx = (int)(intptr_t)lv_event_get_user_data(ev);
            vault_set_deadman_days(DM_DAY_OPTS[idx]);
            lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
            _show_settings();
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    _divider(sheet, 148);

    lv_obj_t* row_dis = lv_obj_create(sheet);
    lv_obj_set_size(row_dis, UI_W, 44); lv_obj_set_pos(row_dis, 0, 150);
    lv_obj_set_style_bg_opa(row_dis, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row_dis, 0, 0);
    lv_obj_set_style_pad_all(row_dis, 0, 0); lv_obj_clear_flag(row_dis, LV_OBJ_FLAG_SCROLLABLE);
    _press_fb(row_dis);
    lv_obj_t* ico_dis = lv_label_create(row_dis);
    lv_label_set_text(ico_dis, ICO_DELETE); lv_obj_set_style_text_font(ico_dis, FONT_ICON, 0);
    lv_obj_set_style_text_color(ico_dis, C_DANGER, 0); lv_obj_set_pos(ico_dis, 16, 14);
    lv_obj_clear_flag(ico_dis, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* lbl_dis = lv_label_create(row_dis);
    lv_label_set_text(lbl_dis, t(K_MP_DESACTIVAR));
    lv_obj_set_style_text_color(lbl_dis, C_DANGER, 0);
    lv_obj_set_style_text_font(lbl_dis, FONT_MD, 0);
    lv_obj_set_pos(lbl_dis, 44, 14); lv_obj_clear_flag(lbl_dis, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row_dis, [](lv_event_t* ev){
        lv_obj_del(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev)));
        vault_set_deadman_days(0);
        _show_settings();
    }, LV_EVENT_CLICKED, nullptr);

    lv_anim_t ash;
    lv_anim_init(&ash); lv_anim_set_var(&ash, sheet);
    lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&ash, UI_H, UI_H - SH);
    lv_anim_set_time(&ash, 220); lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
    lv_anim_start(&ash);
}

// ── PANTALLA: Bloqueo por arrebato — explicación + selector nivel ──
static lv_obj_t* _sn_level_btns[3];

static void _sn_refresh_level_ui(int selected) {
    for (int i = 0; i < 3; i++) {
        if (!_sn_level_btns[i] || !lv_obj_is_valid(_sn_level_btns[i])) continue;
        bool sel = (i + 1 == selected);
        lv_obj_set_style_bg_color(_sn_level_btns[i], sel ? C_ACCENT : C_CARD, 0);
        lv_obj_t* lbl = lv_obj_get_child(_sn_level_btns[i], 0);
        if (lbl) lv_obj_set_style_text_color(lbl, sel ? C_ACCENT_TEXT : C_TEXT, 0);
    }
}

static void _show_snatch_info() {
    _current_screen = SCREEN_PRIVACY; _kb_active = false;

    lv_obj_t* scr = _make_screen();
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bb = lv_btn_create(hdr); lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
    lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0); lv_obj_set_style_shadow_width(bb, 0, 0); lv_obj_set_style_border_width(bb, 0, 0);
    lv_obj_t* bi = lv_label_create(bb); lv_label_set_text(bi, ICO_ARROW_BACK); lv_obj_set_style_text_font(bi, FONT_ICON, 0);
    lv_obj_set_style_text_color(bi, C_TEXT, 0); lv_obj_center(bi);
    _press_fb(bb);
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_settings(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* tt = lv_label_create(hdr); lv_label_set_text(tt, t(K_SNATCH_LOCK));
    lv_obj_set_style_text_color(tt, C_TEXT, 0); lv_obj_set_style_text_font(tt, FONT_LG, 0); lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);
    _divider(scr, 44);

    // Cuerpo scrolleable — deja espacio abajo para los 3 botones de nivel
    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_W, UI_H - 44 - 76); lv_obj_set_pos(body, 0, 44);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0); lv_obj_set_style_pad_top(body, 14, 0); lv_obj_set_style_pad_bottom(body, 14, 0);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* h2 = lv_label_create(body);
    lv_label_set_text(h2, t(K_SNATCH_TITULO));
    lv_obj_set_style_text_color(h2, C_TEXT, 0);
    lv_obj_set_style_text_font(h2, FONT_HDR, 0);
    lv_label_set_long_mode(h2, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(h2, UI_W - 32);
    lv_obj_set_pos(h2, 0, 0);

    lv_obj_t* body_txt = lv_label_create(body);
    lv_label_set_text(body_txt, t(K_SNATCH_BODY));
    lv_obj_set_style_text_color(body_txt, C_MUTED, 0);
    lv_obj_set_style_text_font(body_txt, FONT_SM, 0);
    lv_label_set_long_mode(body_txt, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body_txt, UI_W - 32);
    lv_obj_set_pos(body_txt, 0, 34);

    // Fila fija inferior: elegir nivel activa directamente (no destructivo —
    // en el peor caso un falso positivo solo vuelve a pedir el PIN).
    const char* labels[3] = { t(K_SNATCH_BAJA), t(K_SNATCH_MEDIA), t(K_SNATCH_ALTA) };
    int bw = (UI_W - 32 - 2 * 8) / 3;
    for (int i = 0; i < 3; i++) {
        lv_obj_t* lb = lv_btn_create(scr);
        lv_obj_set_size(lb, bw, 48);
        lv_obj_set_pos(lb, 16 + i * (bw + 8), UI_H - 60);
        lv_obj_set_style_bg_color(lb, C_CARD, 0);
        lv_obj_set_style_radius(lb, 10, 0);
        lv_obj_set_style_shadow_width(lb, 0, 0); lv_obj_set_style_border_width(lb, 0, 0);
        _press_fb(lb);
        lv_obj_t* ll = lv_label_create(lb);
        lv_label_set_text(ll, labels[i]);
        lv_obj_set_style_text_font(ll, FONT_MD, 0);
        lv_obj_center(ll);
        lv_obj_clear_flag(ll, LV_OBJ_FLAG_CLICKABLE);
        _sn_level_btns[i] = lb;
        lv_obj_add_event_cb(lb, [](lv_event_t* ev){
            int lvl = (int)(intptr_t)lv_event_get_user_data(ev) + 1;
            vault_set_snatch_level(lvl);
            _show_settings();
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    _sn_refresh_level_ui(0);

    _load_screen(scr);
}

// ── SHEET: Bloqueo por arrebato ya activo — cambiar nivel / desactivar ──
static void _show_snatch_settings_sheet() {
    lv_obj_t* ov = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ov, UI_W, UI_H); lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    _fade_overlay(ov, LV_OPA_50);
    lv_obj_set_style_border_width(ov, 0, 0); lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ov, [](lv_event_t* ev){
        lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
    }, LV_EVENT_CLICKED, ov);

    const int SH = 220;
    lv_obj_t* sheet = lv_obj_create(ov);
    lv_obj_set_size(sheet, UI_W, SH); lv_obj_set_pos(sheet, 0, UI_H - SH);
    lv_obj_set_style_bg_color(sheet, C_BG, 0); lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sheet, 0, 0); lv_obj_set_style_radius(sheet, 12, 0);
    lv_obj_set_style_pad_all(sheet, 0, 0);
    lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* sh_title = lv_label_create(sheet);
    lv_label_set_text(sh_title, t(K_SNATCH_LOCK));
    lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
    lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
    lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);
    _divider(sheet, 40);

    int cur = vault_get_snatch_level();
    const char* labels[3] = { t(K_SNATCH_BAJA), t(K_SNATCH_MEDIA), t(K_SNATCH_ALTA) };
    int bw = (UI_W - 32 - 2 * 8) / 3;
    for (int i = 0; i < 3; i++) {
        lv_obj_t* lb = lv_btn_create(sheet);
        lv_obj_set_size(lb, bw, 48);
        lv_obj_set_pos(lb, 16 + i * (bw + 8), 54);
        bool sel = (i + 1 == cur);
        lv_obj_set_style_bg_color(lb, sel ? C_ACCENT : C_CARD, 0);
        lv_obj_set_style_radius(lb, 10, 0);
        lv_obj_set_style_shadow_width(lb, 0, 0); lv_obj_set_style_border_width(lb, 0, 0);
        _press_fb(lb);
        lv_obj_t* ll = lv_label_create(lb);
        lv_label_set_text(ll, labels[i]);
        lv_obj_set_style_text_color(ll, sel ? C_ACCENT_TEXT : C_TEXT, 0);
        lv_obj_set_style_text_font(ll, FONT_MD, 0);
        lv_obj_center(ll);
        lv_obj_clear_flag(ll, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(lb, [](lv_event_t* ev){
            int lvl = (int)(intptr_t)lv_event_get_user_data(ev) + 1;
            vault_set_snatch_level(lvl);
            lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
            _show_settings();
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    _divider(sheet, 118);

    lv_obj_t* row_dis = lv_obj_create(sheet);
    lv_obj_set_size(row_dis, UI_W, 44); lv_obj_set_pos(row_dis, 0, 120);
    lv_obj_set_style_bg_opa(row_dis, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row_dis, 0, 0);
    lv_obj_set_style_pad_all(row_dis, 0, 0); lv_obj_clear_flag(row_dis, LV_OBJ_FLAG_SCROLLABLE);
    _press_fb(row_dis);
    lv_obj_t* ico_dis = lv_label_create(row_dis);
    lv_label_set_text(ico_dis, ICO_DELETE); lv_obj_set_style_text_font(ico_dis, FONT_ICON, 0);
    lv_obj_set_style_text_color(ico_dis, C_DANGER, 0); lv_obj_set_pos(ico_dis, 16, 14);
    lv_obj_clear_flag(ico_dis, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* lbl_dis = lv_label_create(row_dis);
    lv_label_set_text(lbl_dis, t(K_SNATCH_DESACTIVAR));
    lv_obj_set_style_text_color(lbl_dis, C_DANGER, 0);
    lv_obj_set_style_text_font(lbl_dis, FONT_MD, 0);
    lv_obj_set_pos(lbl_dis, 44, 14); lv_obj_clear_flag(lbl_dis, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row_dis, [](lv_event_t* ev){
        lv_obj_del(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev)));
        vault_set_snatch_level(0);
        _show_settings();
    }, LV_EVENT_CLICKED, nullptr);

    lv_anim_t ash;
    lv_anim_init(&ash); lv_anim_set_var(&ash, sheet);
    lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&ash, UI_H, UI_H - SH);
    lv_anim_set_time(&ash, 220); lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
    lv_anim_start(&ash);
}

// ── PANTALLA: Acerca de ───────────────────────────────────────
static void _show_about() {
    _current_screen = SCREEN_ABOUT; _kb_active = false;
    lv_obj_t* scr = _make_screen();
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bb = lv_btn_create(hdr); lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
    lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0); lv_obj_set_style_shadow_width(bb, 0, 0); lv_obj_set_style_border_width(bb, 0, 0);
    lv_obj_t* bi = lv_label_create(bb); lv_label_set_text(bi, ICO_ARROW_BACK); lv_obj_set_style_text_font(bi, FONT_ICON, 0);
    lv_obj_set_style_text_color(bi, C_TEXT, 0); lv_obj_center(bi);
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_settings(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* tt = lv_label_create(hdr); lv_label_set_text(tt, t(K_ACERCA_DE));
    lv_obj_set_style_text_color(tt, C_TEXT, 0); lv_obj_set_style_text_font(tt, FONT_LG, 0); lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);
    _divider(scr, 44);

    lv_obj_t* logo = lv_label_create(scr); lv_label_set_text(logo, "Huskey");
    lv_obj_set_style_text_color(logo, C_TEXT, 0); lv_obj_set_style_text_font(logo, FONT_HDR, 0);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 58);

    lv_obj_t* sub = lv_label_create(scr); lv_label_set_text(sub, t(K_GESTOR_HW));
    lv_obj_set_style_text_color(sub, C_MUTED, 0); lv_obj_set_style_text_font(sub, FONT_SM, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 84);

    lv_obj_t* divl = lv_obj_create(scr); lv_obj_set_size(divl, UI_W - 32, 1);
    lv_obj_align(divl, LV_ALIGN_TOP_MID, 0, 108);
    lv_obj_set_style_bg_color(divl, C_BORDER, 0); lv_obj_set_style_bg_opa(divl, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divl, 0, 0); lv_obj_set_style_pad_all(divl, 0, 0);

    int py = 120;
    auto about_row = [&](const char* label, const char* value) {
        lv_obj_t* l = lv_label_create(scr); lv_label_set_text(l, label);
        lv_obj_set_style_text_color(l, C_MUTED, 0); lv_obj_set_style_text_font(l, FONT_SM, 0); lv_obj_set_pos(l, 16, py);
        lv_obj_t* v = lv_label_create(scr); lv_label_set_text(v, value);
        lv_obj_set_style_text_color(v, C_TEXT, 0); lv_obj_set_style_text_font(v, FONT_SM, 0);
        lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -16, py);
        py += 28;
    };
    about_row(t(K_VERSION),          "1.0.0");
    about_row(t(K_DESARROLLADO_POR), "Rango Maker");
    about_row(t(K_ANO),              "2026");
    about_row(t(K_CIFRADO),          "AES-256-CBC");

    lv_obj_t* tag = lv_label_create(scr);
    lv_label_set_text(tag, t(K_TAGLINE));
    lv_obj_set_style_text_color(tag, C_MUTED, 0); lv_obj_set_style_text_font(tag, FONT_SM, 0);
    lv_obj_set_style_text_align(tag, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tag, LV_ALIGN_BOTTOM_MID, 0, -20);
    _load_screen(scr);
}


// ── PANTALLA: Importar / Exportar (CSV plano + .hky cifrado) ──
static lv_timer_t* _ie_import_timer  = nullptr;
static lv_obj_t*    _ie_prog_section = nullptr;
static lv_obj_t*    _ie_idle_section = nullptr;
static lv_obj_t*    _ie_bar_fill     = nullptr;
static lv_obj_t*    _ie_prog_lbl     = nullptr;
static lv_obj_t*    _ie_prog_done_btn = nullptr;
static lv_obj_t*    _ie_export_result = nullptr;
static lv_obj_t*    _ie_delete_export_btn = nullptr;

static char _ie_import_source_path[VAULT_FIELD_LEN] = {0}; // archivo original a borrar al terminar
static char _ie_pw_buf[64] = {0};
static char _ie_pending_import_path[VAULT_FIELD_LEN] = {0};
enum { IE_PW_NONE = 0, IE_PW_EXPORT, IE_PW_IMPORT };
static int _ie_pw_purpose = IE_PW_NONE;

static void _ie_refresh_export_state() {
    bool has_csv = sd_is_ready() && sd_exists(SD_EXPORT_DIR "/huskey_export.csv");
    bool has_hky = sd_is_ready() && sd_exists(SD_EXPORT_DIR "/huskey_backup.hky");
    if (has_csv || has_hky) {
        String msg = t(K_HAY);
        if (has_csv && has_hky) msg += t(K_BACKUP_Y_CSV);
        else if (has_hky)        msg += t(K_BACKUP_HKY);
        else                     msg += t(K_CSV_SIN_CIFRAR);
        msg += t(K_EN_SD_SUFFIX);
        lv_label_set_text(_ie_export_result, msg.c_str());
        lv_obj_clear_flag(_ie_delete_export_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(_ie_export_result, "");
        lv_obj_add_flag(_ie_delete_export_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t* _ie_prog_title = nullptr;
static lv_obj_t* _ie_prog_hint  = nullptr;
static lv_timer_t* _ie_export_timer = nullptr;
static int  _ie_export_mode = 0; // 0=csv, 1=hky
static char _ie_export_pw[64] = {0};

static void _ie_import_tick(lv_timer_t* tmr) {
    bool more = vault_import_csv_step(8); // pocas líneas por tick, no bloquea LVGL/touch

    int imported = 0, errors = 0, lines_done = 0, total_lines = 0; bool active = false;
    vault_import_csv_progress(&imported, &errors, &lines_done, &total_lines, &active);

    if (_ie_bar_fill && lv_obj_is_valid(_ie_bar_fill) && total_lines > 0) {
        int pct = (lines_done * 100) / total_lines;
        if (pct > 100) pct = 100;
        int w = ((UI_W - 64) * pct) / 100;
        lv_obj_set_width(_ie_bar_fill, w);
    }
    if (_ie_prog_lbl && lv_obj_is_valid(_ie_prog_lbl)) {
        char buf[80];
        snprintf(buf, sizeof(buf), t(K_PROG_LINEAS_FMT),
                 lines_done, total_lines, imported, errors);
        lv_label_set_text(_ie_prog_lbl, buf);
    }

    if (!more) {
        lv_timer_del(_ie_import_timer);
        _ie_import_timer = nullptr;

        // Borrado seguro del archivo original importado (CSV o .hky) y
        // del temporal plano que pudo haber quedado del flujo .hky —
        // sd_secure_delete es no-op seguro si el archivo no existe.
        if (_ie_import_source_path[0]) sd_secure_delete(_ie_import_source_path);
        sd_secure_delete(HKY_TMP_PATH);
        memset(_ie_import_source_path, 0, sizeof(_ie_import_source_path));

        if (_ie_prog_hint && lv_obj_is_valid(_ie_prog_hint))
            lv_obj_add_flag(_ie_prog_hint, LV_OBJ_FLAG_HIDDEN);
        if (_ie_prog_done_btn && lv_obj_is_valid(_ie_prog_done_btn))
            lv_obj_clear_flag(_ie_prog_done_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

// Construye la sección de progreso (oculta hasta que arranca un import)
static void _ie_build_progress_section(lv_obj_t* scr) {
    _ie_prog_section = lv_obj_create(scr);
    lv_obj_set_size(_ie_prog_section, UI_W, 160);
    lv_obj_set_pos(_ie_prog_section, 0, 130);
    lv_obj_set_style_bg_opa(_ie_prog_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_ie_prog_section, 0, 0);
    lv_obj_set_style_pad_all(_ie_prog_section, 0, 0);
    lv_obj_clear_flag(_ie_prog_section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_ie_prog_section, LV_OBJ_FLAG_HIDDEN);

    _ie_prog_title = lv_label_create(_ie_prog_section);
    lv_label_set_text(_ie_prog_title, t(K_IMPORTANDO));
    lv_obj_set_style_text_color(_ie_prog_title, C_TEXT, 0);
    lv_obj_set_style_text_font(_ie_prog_title, FONT_MD, 0);
    lv_obj_align(_ie_prog_title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* bar_bg = lv_obj_create(_ie_prog_section);
    lv_obj_set_size(bar_bg, UI_W - 64, 8);
    lv_obj_align(bar_bg, LV_ALIGN_TOP_MID, 0, 32);
    lv_obj_set_style_bg_color(bar_bg, C_BORDER, 0); lv_obj_set_style_bg_opa(bar_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar_bg, 0, 0); lv_obj_set_style_radius(bar_bg, 4, 0);
    lv_obj_set_style_pad_all(bar_bg, 0, 0); lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_SCROLLABLE);

    _ie_bar_fill = lv_obj_create(bar_bg);
    lv_obj_set_size(_ie_bar_fill, 0, 8); lv_obj_set_pos(_ie_bar_fill, 0, 0);
    lv_obj_set_style_bg_color(_ie_bar_fill, C_BTN_BG, 0); lv_obj_set_style_bg_opa(_ie_bar_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_ie_bar_fill, 0, 0); lv_obj_set_style_radius(_ie_bar_fill, 4, 0);
    lv_obj_set_style_pad_all(_ie_bar_fill, 0, 0);

    _ie_prog_lbl = lv_label_create(_ie_prog_section);
    lv_label_set_text(_ie_prog_lbl, "");
    lv_obj_set_style_text_color(_ie_prog_lbl, C_MUTED, 0);
    lv_obj_set_style_text_font(_ie_prog_lbl, FONT_SM, 0);
    lv_obj_set_style_text_align(_ie_prog_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_ie_prog_lbl, LV_ALIGN_TOP_MID, 0, 52);

    _ie_prog_hint = lv_label_create(_ie_prog_section);
    lv_label_set_text(_ie_prog_hint, t(K_NO_REINICIES));
    lv_obj_set_style_text_color(_ie_prog_hint, C_MUTED, 0);
    lv_obj_set_style_text_font(_ie_prog_hint, FONT_SM, 0);
    lv_obj_set_style_text_align(_ie_prog_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_ie_prog_hint, LV_ALIGN_TOP_MID, 0, 76);

    _ie_prog_done_btn = lv_btn_create(_ie_prog_section);
    lv_obj_set_size(_ie_prog_done_btn, 120, 36);
    lv_obj_align(_ie_prog_done_btn, LV_ALIGN_TOP_MID, 0, 108);
    lv_obj_set_style_bg_color(_ie_prog_done_btn, C_ACCENT, 0); lv_obj_set_style_radius(_ie_prog_done_btn, 8, 0);
    lv_obj_add_flag(_ie_prog_done_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* done_lbl = lv_label_create(_ie_prog_done_btn);
    lv_label_set_text(done_lbl, t(K_LISTO));
    lv_obj_set_style_text_color(done_lbl, C_ACCENT_TEXT, 0); lv_obj_set_style_text_font(done_lbl, FONT_SM, 0);
    lv_obj_center(done_lbl);
    lv_obj_add_event_cb(_ie_prog_done_btn, [](lv_event_t* e){
        if (_ie_prog_section)  lv_obj_add_flag(_ie_prog_section, LV_OBJ_FLAG_HIDDEN);
        if (_ie_idle_section)  lv_obj_clear_flag(_ie_idle_section, LV_OBJ_FLAG_HIDDEN);
        _ie_refresh_export_state();
    }, LV_EVENT_CLICKED, nullptr);
}

// Muestra la sección de progreso (compartida por CSV y .hky)
static void _ie_show_import_progress_ui() {
    if (_ie_prog_title)    lv_label_set_text(_ie_prog_title, t(K_IMPORTANDO));
    if (_ie_prog_hint)     lv_obj_clear_flag(_ie_prog_hint, LV_OBJ_FLAG_HIDDEN);
    if (_ie_idle_section)  lv_obj_add_flag(_ie_idle_section, LV_OBJ_FLAG_HIDDEN);
    if (_ie_prog_section)  lv_obj_clear_flag(_ie_prog_section, LV_OBJ_FLAG_HIDDEN);
    if (_ie_prog_done_btn) lv_obj_add_flag(_ie_prog_done_btn, LV_OBJ_FLAG_HIDDEN);
    if (_ie_bar_fill)      lv_obj_set_width(_ie_bar_fill, 0);
    if (_ie_prog_lbl)      lv_label_set_text(_ie_prog_lbl, "");
}

static void _ie_import_begin_failed(const char* msg) {
    if (_ie_prog_lbl) lv_label_set_text(_ie_prog_lbl, msg);
    if (_ie_prog_hint) lv_obj_add_flag(_ie_prog_hint, LV_OBJ_FLAG_HIDDEN);
    if (_ie_prog_done_btn) lv_obj_clear_flag(_ie_prog_done_btn, LV_OBJ_FLAG_HIDDEN);
    memset(_ie_import_source_path, 0, sizeof(_ie_import_source_path));
}

static void _ie_start_import(const char* path) {
    strncpy(_ie_import_source_path, path, VAULT_FIELD_LEN - 1);
    // Diferir begin en un timer para permitir el redibujado de la UI antes de la lectura
    _ie_show_import_progress_ui();
    lv_timer_t* kick = lv_timer_create([](lv_timer_t* tm){
        if (!vault_import_csv_begin(_ie_import_source_path)) {
            _ie_import_begin_failed(t(K_ERR_ARCHIVO_FMT));
            return;
        }
        if (_ie_import_timer) lv_timer_del(_ie_import_timer);
        _ie_import_timer = lv_timer_create(_ie_import_tick, 60, nullptr);
    }, 50, nullptr);
    lv_timer_set_repeat_count(kick, 1);
}

static char _ie_hky_pw[64] = {0};
static void _ie_hky_begin_done(int rc, const char* path); // fwd

static void _ie_try_import_hky(const char* path, const char* password) {
    // Diferir begin en un timer para renderizar el progreso antes de la derivación PBKDF2
    strncpy(_ie_import_source_path, path, VAULT_FIELD_LEN - 1);
    strncpy(_ie_hky_pw, password, sizeof(_ie_hky_pw) - 1);
    _ie_show_import_progress_ui();

    lv_timer_t* kick = lv_timer_create([](lv_timer_t* tm){
        char path_l[VAULT_FIELD_LEN];
        strncpy(path_l, _ie_import_source_path, VAULT_FIELD_LEN - 1);
        path_l[VAULT_FIELD_LEN - 1] = '\0';
        int rc = vault_import_hky_begin(path_l, _ie_hky_pw);
        memset(_ie_hky_pw, 0, sizeof(_ie_hky_pw));
        _ie_hky_begin_done(rc, path_l);
    }, 50, nullptr);
    lv_timer_set_repeat_count(kick, 1);
}

// Resultado del begin diferido del .hky
static void _ie_hky_begin_done(int rc, const char* path) {
    if (rc == 0) {
        if (_ie_import_timer) lv_timer_del(_ie_import_timer);
        _ie_import_timer = lv_timer_create(_ie_import_tick, 60, nullptr);
    } else if (rc == -2) {
        // Contraseña incorrecta — restaurar la vista y reintentar sin
        // perder el archivo elegido.
        if (_ie_prog_section) lv_obj_add_flag(_ie_prog_section, LV_OBJ_FLAG_HIDDEN);
        if (_ie_idle_section) lv_obj_clear_flag(_ie_idle_section, LV_OBJ_FLAG_HIDDEN);
        strncpy(_ie_pending_import_path, path, VAULT_FIELD_LEN - 1);
        _ie_pw_purpose = IE_PW_IMPORT;
        memset(_ie_pw_buf, 0, sizeof(_ie_pw_buf));
        _show_field_editor(t(K_PASSWORD_INCORRECTA_REINTENTA), _ie_pw_buf, sizeof(_ie_pw_buf), true, [](){
            char path_copy[VAULT_FIELD_LEN];
            strncpy(path_copy, _ie_pending_import_path, VAULT_FIELD_LEN - 1);
            char pw_copy[64];
            strncpy(pw_copy, _ie_pw_buf, sizeof(pw_copy) - 1);
            memset(_ie_pw_buf, 0, sizeof(_ie_pw_buf));
            _ie_pw_purpose = IE_PW_NONE;
            _ie_try_import_hky(path_copy, pw_copy);
        });
    } else {
        _ie_import_begin_failed(t(K_ERR_BACKUP_INVALIDO));
    }
}

static void _ie_export_tick(lv_timer_t* tmr) {
    bool more = _ie_export_mode == 1 ? vault_export_hky_step(6)
                                     : vault_export_csv_step(6);

    int done = 0, total = 0; bool active = false;
    if (_ie_export_mode == 1) vault_export_hky_progress(&done, &total, &active);
    else                      vault_export_csv_progress(&done, &total, &active);

    if (_ie_bar_fill && lv_obj_is_valid(_ie_bar_fill) && total > 0) {
        int pct = (done * 100) / total;
        if (pct > 100) pct = 100;
        lv_obj_set_width(_ie_bar_fill, ((UI_W - 64) * pct) / 100);
    }
    if (_ie_prog_lbl && lv_obj_is_valid(_ie_prog_lbl)) {
        char buf[48];
        snprintf(buf, sizeof(buf), t(K_PROG_EXPORT_FMT), done, total);
        lv_label_set_text(_ie_prog_lbl, buf);
    }

    if (!more) {
        lv_timer_del(_ie_export_timer);
        _ie_export_timer = nullptr;

        int n = _ie_export_mode == 1 ? vault_export_hky_result()
                                     : vault_export_csv_result();
        char buf[112];
        if (n < 0) snprintf(buf, sizeof(buf), "%s",
            t(_ie_export_mode == 1 ? K_ERROR_EXPORTAR_BACKUP : K_ERROR_EXPORTAR));
        else snprintf(buf, sizeof(buf),
            t(_ie_export_mode == 1 ? K_BACKUP_CIFRADO_FMT : K_EXPORTADO_SIN_CIFRAR_FMT), n);
        if (_ie_prog_lbl) lv_label_set_text(_ie_prog_lbl, buf);
        if (_ie_prog_hint) lv_obj_add_flag(_ie_prog_hint, LV_OBJ_FLAG_HIDDEN);
        if (_ie_bar_fill && n >= 0) lv_obj_set_width(_ie_bar_fill, UI_W - 64);
        if (_ie_prog_done_btn) lv_obj_clear_flag(_ie_prog_done_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

// Inicia export incremental (0=csv, 1=hky) difiriendo el begin para actualizar la UI
static void _ie_start_export(int mode, const char* password) {
    if (!sd_is_ready()) { lv_label_set_text(_ie_export_result, t(K_SD_NO_DISPONIBLE)); return; }
    _ie_export_mode = mode;
    if (password) strncpy(_ie_export_pw, password, sizeof(_ie_export_pw) - 1);
    else          memset(_ie_export_pw, 0, sizeof(_ie_export_pw));

    if (_ie_prog_title)    lv_label_set_text(_ie_prog_title, t(K_EXPORTANDO));
    if (_ie_prog_hint)     lv_obj_clear_flag(_ie_prog_hint, LV_OBJ_FLAG_HIDDEN);
    if (_ie_idle_section)  lv_obj_add_flag(_ie_idle_section, LV_OBJ_FLAG_HIDDEN);
    if (_ie_prog_section)  lv_obj_clear_flag(_ie_prog_section, LV_OBJ_FLAG_HIDDEN);
    if (_ie_prog_done_btn) lv_obj_add_flag(_ie_prog_done_btn, LV_OBJ_FLAG_HIDDEN);
    if (_ie_bar_fill)      lv_obj_set_width(_ie_bar_fill, 0);
    if (_ie_prog_lbl)      lv_label_set_text(_ie_prog_lbl, "");

    lv_timer_t* kick = lv_timer_create([](lv_timer_t* tm){
        bool ok = _ie_export_mode == 1
            ? vault_export_hky_begin(SD_EXPORT_DIR "/huskey_backup.hky", _ie_export_pw)
            : vault_export_csv_begin(SD_EXPORT_DIR "/huskey_export.csv");
        memset(_ie_export_pw, 0, sizeof(_ie_export_pw));
        if (!ok) {
            if (_ie_prog_lbl) lv_label_set_text(_ie_prog_lbl,
                t(_ie_export_mode == 1 ? K_ERROR_EXPORTAR_BACKUP : K_ERROR_EXPORTAR));
            if (_ie_prog_done_btn) lv_obj_clear_flag(_ie_prog_done_btn, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (_ie_export_timer) lv_timer_del(_ie_export_timer);
        _ie_export_timer = lv_timer_create(_ie_export_tick, 40, nullptr);
    }, 50, nullptr);
    lv_timer_set_repeat_count(kick, 1);
}

static void _ie_do_export_hky(const char* password) {
    _ie_start_export(1, password);
}

// Bottom sheet: lista de .csv/.hky encontrados en la raíz de la SD
static void _ie_show_import_picker() {
    static char files[20][VAULT_FIELD_LEN];
    int count = sd_is_ready() ? sd_list_import_files_root(files, 20) : 0;

    lv_obj_t* ov = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ov, UI_W, UI_H); lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    _fade_overlay(ov, LV_OPA_50);
    lv_obj_set_style_border_width(ov, 0, 0); lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ov, [](lv_event_t* ev){
        lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
    }, LV_EVENT_CLICKED, ov);

    int SH = min(UI_H - 44, 64 + max(count, 1) * 44);
    lv_obj_t* sheet = lv_obj_create(ov);
    lv_obj_set_size(sheet, UI_W, SH);
    lv_obj_set_pos(sheet, 0, UI_H - SH);
    lv_obj_set_style_bg_color(sheet, C_BG, 0); lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sheet, 0, 0); lv_obj_set_style_radius(sheet, 12, 0);
    lv_obj_set_style_pad_all(sheet, 0, 0);
    lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* hndl = lv_obj_create(sheet);
    lv_obj_set_size(hndl, 40, 4); lv_obj_align(hndl, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(hndl, C_BORDER, 0); lv_obj_set_style_bg_opa(hndl, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hndl, 0, 0); lv_obj_set_style_radius(hndl, 2, 0);

    lv_obj_t* shtl = lv_label_create(sheet);
    lv_label_set_text(shtl, t(K_ELEGIR_ARCHIVO));
    lv_obj_set_style_text_color(shtl, C_TEXT, 0); lv_obj_set_style_text_font(shtl, FONT_MD, 0);
    lv_obj_align(shtl, LV_ALIGN_TOP_MID, 0, 18);
    _divider(sheet, 38);

    if (count == 0) {
        lv_obj_t* empty = lv_label_create(sheet);
        lv_label_set_text(empty, sd_is_ready() ? t(K_NO_HAY_ARCHIVOS_IMPORT) : t(K_SD_NO_DISPONIBLE));
        lv_obj_set_style_text_color(empty, C_MUTED, 0); lv_obj_set_style_text_font(empty, FONT_SM, 0);
        lv_obj_align(empty, LV_ALIGN_TOP_MID, 0, 56);
    }

    for (int i = 0; i < count; i++) {
        lv_obj_t* row = lv_obj_create(sheet);
        lv_obj_set_size(row, UI_W, 40);
        lv_obj_set_pos(row, 0, 46 + i * 40);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        // Mostrar solo el nombre del archivo, sin la "/" inicial
        const char* fname = files[i][0] == '/' ? files[i] + 1 : files[i];
        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, fname);
        lv_obj_set_style_text_font(lbl, FONT_SM, 0);
        lv_obj_set_style_text_color(lbl, C_TEXT, 0);
        lv_obj_set_pos(lbl, 16, 12);

        bool is_hky = strstr(fname, ".hky") != nullptr;
        if (is_hky) {
            lv_obj_t* lock_ico = lv_label_create(row);
            lv_label_set_text(lock_ico, ICO_LOCK);
            lv_obj_set_style_text_font(lock_ico, FONT_ICON, 0);
            lv_obj_set_style_text_color(lock_ico, C_MUTED, 0);
            lv_obj_align(lock_ico, LV_ALIGN_RIGHT_MID, -16, 0);
        }

        lv_obj_add_event_cb(row, [](lv_event_t* ev){
            int idx = (int)(intptr_t)lv_event_get_user_data(ev);
            lv_obj_t* ov_ref = (lv_obj_t*)lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev)));
            char path[VAULT_FIELD_LEN];
            strncpy(path, files[idx], VAULT_FIELD_LEN - 1);
            lv_obj_del(ov_ref);

            String p = String(path); p.toLowerCase();
            if (p.endsWith(".hky")) {
                strncpy(_ie_pending_import_path, path, VAULT_FIELD_LEN - 1);
                _ie_pw_purpose = IE_PW_IMPORT;
                memset(_ie_pw_buf, 0, sizeof(_ie_pw_buf));
                _show_field_editor(t(K_PASSWORD_DEL_BACKUP), _ie_pw_buf, sizeof(_ie_pw_buf), true, [](){
                    char path_copy[VAULT_FIELD_LEN];
                    strncpy(path_copy, _ie_pending_import_path, VAULT_FIELD_LEN - 1);
                    char pw_copy[64];
                    strncpy(pw_copy, _ie_pw_buf, sizeof(pw_copy) - 1);
                    memset(_ie_pw_buf, 0, sizeof(_ie_pw_buf));
                    _ie_pw_purpose = IE_PW_NONE;
                    _ie_try_import_hky(path_copy, pw_copy);
                });
            } else {
                _ie_start_import(path);
            }
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

static void _show_import_export() {
    _current_screen = SCREEN_IMPORT_EXPORT; _kb_active = false;
    if (!sd_is_ready()) sd_begin(); // intentar montar si no se hizo antes
    sd_ensure_io_dirs();

    lv_obj_t* scr = _make_screen();
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bb = lv_btn_create(hdr); lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
    lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0); lv_obj_set_style_shadow_width(bb, 0, 0); lv_obj_set_style_border_width(bb, 0, 0);
    lv_obj_t* bi = lv_label_create(bb); lv_label_set_text(bi, ICO_ARROW_BACK); lv_obj_set_style_text_font(bi, FONT_ICON, 0);
    lv_obj_set_style_text_color(bi, C_TEXT, 0); lv_obj_align(bi, LV_ALIGN_CENTER, 0, 8);
    lv_obj_add_event_cb(bb, [](lv_event_t* e){
        if (_ie_import_timer) { lv_timer_del(_ie_import_timer); _ie_import_timer = nullptr; vault_import_csv_cancel(); }
        _show_settings();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* tt = lv_label_create(hdr); lv_label_set_text(tt, t(K_IMPORTAR_EXPORTAR));
    lv_obj_set_style_text_color(tt, C_TEXT, 0); lv_obj_set_style_text_font(tt, FONT_MD, 0); lv_obj_align(tt, LV_ALIGN_CENTER, 0, 8);
    _divider(scr, 44);

    // ── Sección normal (export + elegir import) — scrollable, ya
    // que con dos opciones de export el contenido no cabe en 276px.
    _ie_idle_section = lv_obj_create(scr);
    lv_obj_set_size(_ie_idle_section, UI_W, UI_H - 44);
    lv_obj_set_pos(_ie_idle_section, 0, 44);
    lv_obj_set_style_bg_opa(_ie_idle_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_ie_idle_section, 0, 0);
    lv_obj_set_style_pad_all(_ie_idle_section, 0, 0);
    lv_obj_set_scrollbar_mode(_ie_idle_section, LV_SCROLLBAR_MODE_ACTIVE);

    // ── Tarjeta: Exportar (fila completa clickable) ─────────────
    lv_obj_t* card_exp = lv_obj_create(_ie_idle_section);
    lv_obj_set_size(card_exp, UI_W - 32, 80); lv_obj_set_pos(card_exp, 16, 10);
    lv_obj_set_style_bg_color(card_exp, C_CARD, 0); lv_obj_set_style_bg_opa(card_exp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card_exp, 0, 0); lv_obj_set_style_radius(card_exp, 14, 0);
    lv_obj_set_style_pad_all(card_exp, 0, 0); lv_obj_clear_flag(card_exp, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* exp_circ = lv_obj_create(card_exp);
    lv_obj_set_size(exp_circ, 36, 36); lv_obj_set_pos(exp_circ, 10, 22);
    lv_obj_set_style_bg_color(exp_circ, C_BORDER, 0); lv_obj_set_style_bg_opa(exp_circ, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(exp_circ, 0, 0); lv_obj_set_style_radius(exp_circ, 18, 0);
    lv_obj_set_style_pad_all(exp_circ, 0, 0); lv_obj_clear_flag(exp_circ, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* exp_ico = lv_label_create(exp_circ);
    lv_label_set_text(exp_ico, ICO_UPLOAD); lv_obj_set_style_text_font(exp_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(exp_ico, C_TEXT, 0); lv_obj_center(exp_ico);

    lv_obj_t* exp_lbl = lv_label_create(card_exp);
    lv_label_set_text(exp_lbl, t(K_EXPORTAR_ENTRADAS));
    lv_obj_set_style_text_color(exp_lbl, C_TEXT, 0); lv_obj_set_style_text_font(exp_lbl, FONT_MD, 0);
    lv_obj_set_pos(exp_lbl, 56, 15);

    lv_obj_t* exp_sub = lv_label_create(card_exp);
    lv_label_set_text(exp_sub, t(K_GUARDA_ENTRADAS_SUB));
    lv_obj_set_style_text_color(exp_sub, C_MUTED, 0); lv_obj_set_style_text_font(exp_sub, FONT_SM, 0);
    lv_obj_set_pos(exp_sub, 56, 36);
    lv_obj_set_width(exp_sub, UI_W - 32 - 56 - 24);

    lv_obj_t* exp_arr = lv_label_create(card_exp);
    lv_label_set_text(exp_arr, ICO_CHEVRON_RIGHT); lv_obj_set_style_text_font(exp_arr, FONT_ICON, 0);
    lv_obj_set_style_text_color(exp_arr, C_MUTED, 0);
    lv_obj_set_pos(exp_arr, UI_W - 32 - 24, 33);

    lv_obj_add_flag(card_exp, LV_OBJ_FLAG_CLICKABLE);
    _press_fb(card_exp);
    lv_obj_t* btn_exp = card_exp; // alias: la tarjeta entera dispara el sheet de export

    // ── Sheet de opciones de export — mismo estilo que el FAB de
    // "Nueva entrada" (ui_list.h): overlay + sheet con dos filas.
    lv_obj_add_event_cb(btn_exp, [](lv_event_t* e){
        lv_obj_t* scr_act = lv_scr_act();

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

        const int SH = 190;
        lv_obj_t* sheet = lv_obj_create(overlay);
        lv_obj_set_size(sheet, UI_W, SH);
        lv_obj_set_pos(sheet, 0, UI_H - SH);
        lv_obj_set_style_bg_color(sheet, C_BG, 0);
        lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sheet, 0, 0);
        lv_obj_set_style_radius(sheet, 12, 0);
        lv_obj_set_style_pad_all(sheet, 0, 0);
        lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(sheet, [](lv_event_t* ev){
            lv_event_stop_bubbling(ev);
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* sh_title = lv_label_create(sheet);
        lv_label_set_text(sh_title, t(K_EXPORTAR_BOVEDA));
        lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
        lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
        lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);

        _divider(sheet, 40);

        // Opción: backup cifrado (.hky) — recomendado
        lv_obj_t* row_hky = lv_obj_create(sheet);
        lv_obj_set_size(row_hky, UI_W, 54);
        lv_obj_set_pos(row_hky, 0, 46);
        lv_obj_set_style_bg_opa(row_hky, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_hky, 0, 0);
        lv_obj_set_style_pad_all(row_hky, 0, 0);
        lv_obj_set_style_radius(row_hky, 0, 0);
        lv_obj_clear_flag(row_hky, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ico_hky = lv_label_create(row_hky);
        lv_label_set_text(ico_hky, ICO_LOCK); lv_obj_set_style_text_font(ico_hky, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico_hky, C_TEXT, 0);
        lv_obj_set_pos(ico_hky, 16, 18);
        lv_obj_clear_flag(ico_hky, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* lbl_hky = lv_label_create(row_hky);
        lv_label_set_text(lbl_hky, t(K_ARCHIVO_CIFRADO_HKY));
        lv_obj_set_style_text_color(lbl_hky, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl_hky, FONT_MD, 0);
        lv_obj_set_pos(lbl_hky, 42, 6);
        lv_obj_clear_flag(lbl_hky, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* sub_hky = lv_label_create(row_hky);
        lv_label_set_text(sub_hky, t(K_RECOMENDADO_PIDE_PW));
        lv_obj_set_style_text_color(sub_hky, C_MUTED, 0);
        lv_obj_set_style_text_font(sub_hky, FONT_SM, 0);
        lv_obj_set_pos(sub_hky, 42, 28);
        lv_obj_clear_flag(sub_hky, LV_OBJ_FLAG_CLICKABLE);
        _press_fb(row_hky);
        lv_obj_add_event_cb(row_hky, [](lv_event_t* ev){
            lv_obj_t* row = (lv_obj_t*)lv_event_get_target(ev);
            lv_obj_del(lv_obj_get_parent(lv_obj_get_parent(row))); // borra el overlay completo, no solo el sheet
            _ie_pw_purpose = IE_PW_EXPORT;
            memset(_ie_pw_buf, 0, sizeof(_ie_pw_buf));
            _show_field_editor(t(K_PASSWORD_PARA_BACKUP), _ie_pw_buf, sizeof(_ie_pw_buf), true, [](){
                char pw_copy[64];
                strncpy(pw_copy, _ie_pw_buf, sizeof(pw_copy) - 1);
                memset(_ie_pw_buf, 0, sizeof(_ie_pw_buf));
                _ie_pw_purpose = IE_PW_NONE;
                if (strlen(pw_copy) == 0) { lv_label_set_text(_ie_export_result, t(K_CANCELADO_SIN_PW)); return; }
                _ie_do_export_hky(pw_copy);
            });
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* div = lv_obj_create(sheet);
        lv_obj_set_size(div, UI_W - 32, 1);
        lv_obj_set_pos(div, 16, 100);
        lv_obj_set_style_bg_color(div, C_BORDER, 0);
        lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(div, 0, 0);
        lv_obj_set_style_pad_all(div, 0, 0);

        // Opción: CSV plano — advertencia antes de generar
        lv_obj_t* row_csv = lv_obj_create(sheet);
        lv_obj_set_size(row_csv, UI_W, 54);
        lv_obj_set_pos(row_csv, 0, 102);
        lv_obj_set_style_bg_opa(row_csv, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_csv, 0, 0);
        lv_obj_set_style_pad_all(row_csv, 0, 0);
        lv_obj_set_style_radius(row_csv, 0, 0);
        lv_obj_clear_flag(row_csv, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ico_csv = lv_label_create(row_csv);
        lv_label_set_text(ico_csv, ICO_DOWNLOAD); lv_obj_set_style_text_font(ico_csv, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico_csv, C_DANGER, 0);
        lv_obj_set_pos(ico_csv, 16, 18);
        lv_obj_clear_flag(ico_csv, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* lbl_csv = lv_label_create(row_csv);
        lv_label_set_text(lbl_csv, t(K_TEXTO_PLANO_CSV));
        lv_obj_set_style_text_color(lbl_csv, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl_csv, FONT_MD, 0);
        lv_obj_set_pos(lbl_csv, 42, 6);
        lv_obj_clear_flag(lbl_csv, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* sub_csv = lv_label_create(row_csv);
        lv_label_set_text(sub_csv, t(K_SIN_CIFRAR_SUB));
        lv_obj_set_style_text_color(sub_csv, C_MUTED, 0);
        lv_obj_set_style_text_font(sub_csv, FONT_SM, 0);
        lv_obj_set_pos(sub_csv, 42, 28);
        lv_obj_clear_flag(sub_csv, LV_OBJ_FLAG_CLICKABLE);
        _press_fb(row_csv);
        lv_obj_add_event_cb(row_csv, [](lv_event_t* ev){
            lv_obj_t* row = (lv_obj_t*)lv_event_get_target(ev);
            lv_obj_del(lv_obj_get_parent(lv_obj_get_parent(row))); // borra el overlay completo, no solo el sheet

            lv_obj_t* overlay2 = lv_obj_create(lv_scr_act());
            lv_obj_set_size(overlay2, UI_W, UI_H); lv_obj_set_pos(overlay2, 0, 0);
            lv_obj_set_style_bg_color(overlay2, lv_color_hex(0x000000), 0);
            _fade_overlay(overlay2, LV_OPA_60);
            lv_obj_set_style_border_width(overlay2, 0, 0);
            lv_obj_set_style_pad_all(overlay2, 0, 0);
            lv_obj_clear_flag(overlay2, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* box = lv_obj_create(overlay2);
            lv_obj_set_size(box, UI_W - 48, 190);
            lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_bg_color(box, C_CARD, 0); lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(box, 0, 0); lv_obj_set_style_radius(box, 12, 0);
            lv_obj_set_style_pad_all(box, 16, 0);
            lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(box, [](lv_event_t* ev2){ lv_event_stop_bubbling(ev2); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* msg = lv_label_create(box);
            lv_label_set_text(msg, t(K_CSV_WARNING_MSG));
            lv_obj_set_style_text_color(msg, C_TEXT, 0);
            lv_obj_set_style_text_font(msg, FONT_SM, 0);
            lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 0);

            int bw = (UI_W - 48 - 32 - 8) / 2;
            lv_obj_t* btn_no = lv_btn_create(box);
            _press_fb(btn_no);
            lv_obj_set_size(btn_no, bw, 36); lv_obj_set_pos(btn_no, 0, 126);
            lv_obj_set_style_bg_color(btn_no, C_BORDER, 0); lv_obj_set_style_radius(btn_no, 8, 0);
            lv_obj_set_style_shadow_width(btn_no, 0, 0); lv_obj_set_style_border_width(btn_no, 1, 0);
            lv_obj_set_style_border_color(btn_no, C_MUTED, 0);
            lv_obj_t* no_lbl = lv_label_create(btn_no); lv_label_set_text(no_lbl, t(K_CANCELAR));
            lv_obj_set_style_text_color(no_lbl, C_TEXT, 0); lv_obj_set_style_text_font(no_lbl, FONT_SM, 0);
            lv_obj_center(no_lbl);
            lv_obj_add_event_cb(btn_no, [](lv_event_t* ev2){
                lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev2))));
            }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* btn_ok = lv_btn_create(box);
            _press_fb(btn_ok);
            lv_obj_set_size(btn_ok, bw, 36); lv_obj_set_pos(btn_ok, bw + 8, 126);
            lv_obj_set_style_bg_color(btn_ok, C_DANGER, 0); lv_obj_set_style_radius(btn_ok, 8, 0);
            lv_obj_set_style_shadow_width(btn_ok, 0, 0); lv_obj_set_style_border_width(btn_ok, 0, 0);
            lv_obj_t* ok_lbl = lv_label_create(btn_ok); lv_label_set_text(ok_lbl, t(K_EXPORTAR));
            lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ok_lbl, FONT_SM, 0); lv_obj_center(ok_lbl);
            lv_obj_add_event_cb(btn_ok, [](lv_event_t* ev2){
                lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev2))));
                _ie_start_export(0, nullptr);
            }, LV_EVENT_CLICKED, nullptr);
        }, LV_EVENT_CLICKED, nullptr);

        // Animación entrada
        lv_anim_t ash;
        lv_anim_init(&ash);
        lv_anim_set_var(&ash, sheet);
        lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&ash, UI_H, UI_H - SH);
        lv_anim_set_time(&ash, 220);
        lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
        lv_anim_start(&ash);
    }, LV_EVENT_CLICKED, nullptr);

    // Aviso de export pendiente y link de borrado
    _ie_export_result = lv_label_create(_ie_idle_section);
    lv_label_set_text(_ie_export_result, "");
    lv_obj_set_style_text_color(_ie_export_result, C_MUTED, 0); lv_obj_set_style_text_font(_ie_export_result, FONT_SM, 0);
    lv_obj_set_style_text_align(_ie_export_result, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(_ie_export_result, UI_W - 32);
    lv_obj_set_pos(_ie_export_result, 16, 196);

    // Botón compacto (estilo link, no lv_btn completo) para borrar
    // cualquier export presente — oculto si no hay ninguno.
    _ie_delete_export_btn = lv_label_create(_ie_idle_section);
    lv_label_set_text(_ie_delete_export_btn, t(K_BORRAR_EXPORTS_SD));
    lv_obj_set_style_text_color(_ie_delete_export_btn, C_DANGER, 0);
    lv_obj_set_style_text_font(_ie_delete_export_btn, FONT_SM, 0);
    lv_obj_set_style_text_decor(_ie_delete_export_btn, LV_TEXT_DECOR_UNDERLINE, 0);
    lv_obj_set_style_text_align(_ie_delete_export_btn, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(_ie_delete_export_btn, UI_W - 32);
    lv_obj_set_pos(_ie_delete_export_btn, 16, 253);
    lv_obj_add_flag(_ie_delete_export_btn, LV_OBJ_FLAG_CLICKABLE);
    _press_fb(_ie_delete_export_btn);
    lv_obj_add_flag(_ie_delete_export_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(_ie_delete_export_btn, [](lv_event_t* e){
        sd_secure_delete_export_dir(); // borra TODO lo que haya en /export
        lv_label_set_text(_ie_export_result, t(K_EXPORTS_BORRADOS));
        lv_obj_add_flag(_ie_delete_export_btn, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, nullptr);

    _ie_refresh_export_state(); // detectar export(s) de una sesión anterior

    // ── Tarjeta: Importar (fila completa clickable) ─────────────
    lv_obj_t* card_imp = lv_obj_create(_ie_idle_section);
    lv_obj_set_size(card_imp, UI_W - 32, 80); lv_obj_set_pos(card_imp, 16, 105);
    lv_obj_set_style_bg_color(card_imp, C_CARD, 0); lv_obj_set_style_bg_opa(card_imp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card_imp, 0, 0); lv_obj_set_style_radius(card_imp, 14, 0);
    lv_obj_set_style_pad_all(card_imp, 0, 0); lv_obj_clear_flag(card_imp, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* imp_circ = lv_obj_create(card_imp);
    lv_obj_set_size(imp_circ, 36, 36); lv_obj_set_pos(imp_circ, 10, 22);
    lv_obj_set_style_bg_color(imp_circ, C_BORDER, 0); lv_obj_set_style_bg_opa(imp_circ, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(imp_circ, 0, 0); lv_obj_set_style_radius(imp_circ, 18, 0);
    lv_obj_set_style_pad_all(imp_circ, 0, 0); lv_obj_clear_flag(imp_circ, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* imp_ico = lv_label_create(imp_circ);
    lv_label_set_text(imp_ico, ICO_DOWNLOAD); lv_obj_set_style_text_font(imp_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(imp_ico, C_TEXT, 0); lv_obj_center(imp_ico);

    lv_obj_t* imp_lbl = lv_label_create(card_imp);
    lv_label_set_text(imp_lbl, t(K_IMPORTAR_ENTRADAS));
    lv_obj_set_style_text_color(imp_lbl, C_TEXT, 0); lv_obj_set_style_text_font(imp_lbl, FONT_MD, 0);
    lv_obj_set_pos(imp_lbl, 56, 15);

    lv_obj_t* imp_sub = lv_label_create(card_imp);
    lv_label_set_text(imp_sub, t(K_RESTAURA_ARCHIVO_SUB));
    lv_obj_set_style_text_color(imp_sub, C_MUTED, 0); lv_obj_set_style_text_font(imp_sub, FONT_SM, 0);
    lv_obj_set_pos(imp_sub, 56, 36);
    lv_obj_set_width(imp_sub, UI_W - 32 - 56 - 24);

    lv_obj_t* imp_arr = lv_label_create(card_imp);
    lv_label_set_text(imp_arr, ICO_CHEVRON_RIGHT); lv_obj_set_style_text_font(imp_arr, FONT_ICON, 0);
    lv_obj_set_style_text_color(imp_arr, C_MUTED, 0);
    lv_obj_set_pos(imp_arr, UI_W - 32 - 24, 33);

    lv_obj_add_flag(card_imp, LV_OBJ_FLAG_CLICKABLE);
    _press_fb(card_imp);
    lv_obj_add_event_cb(card_imp, [](lv_event_t* e){ _ie_show_import_picker(); }, LV_EVENT_CLICKED, nullptr);

    // ── Sección de progreso (oculta hasta iniciar un import) ──
    _ie_build_progress_section(scr);

    _load_screen(scr);
}

// ── PANTALLA: Cambiar PIN ─────────────────────────────────────
static void _show_change_pin() {
    _current_screen = SCREEN_CHANGE_PIN; _kb_active = false;
    static uint8_t _cp_step = 0;
    static char _cp_new[PIN_DIGITS + 1]   = {0};
    static char _cp_input[PIN_DIGITS + 1] = {0};
    static int  _cp_len      = 0;
    _cp_step = 0;
    memset(_cp_new,   0, PIN_DIGITS + 1);
    memset(_cp_input, 0, PIN_DIGITS + 1);
    _cp_len = 0;

    lv_obj_t* scr = _make_screen();

    // Ícono
    lv_obj_t* lock_ico = lv_label_create(scr);
    lv_label_set_text(lock_ico, ICO_LOCK);
    lv_obj_set_style_text_font(lock_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(lock_ico, C_ACCENT, 0);
    lv_obj_align(lock_ico, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t* t2 = lv_label_create(scr);
    lv_label_set_text(t2, t(K_INGRESA_PIN_ACTUAL));
    lv_obj_set_style_text_color(t2, C_TEXT, 0);
    lv_obj_set_style_text_font(t2, FONT_HDR, 0);
    lv_obj_align(t2, LV_ALIGN_TOP_MID, 0, 40);

    int dot_d = 14, dot_gap = 24;
    int dot_tot = PIN_DIGITS*dot_d + (PIN_DIGITS-1)*dot_gap;
    int dot_x0 = (UI_W - dot_tot) / 2;
    int dot_y = 88;

    static lv_obj_t* cp_dots[PIN_DIGITS];
    for (int i = 0; i < PIN_DIGITS; i++) {
        cp_dots[i] = lv_obj_create(scr);
        lv_obj_set_size(cp_dots[i], dot_d, dot_d);
        lv_obj_set_pos(cp_dots[i], dot_x0 + i*(dot_d+dot_gap), dot_y);
        lv_obj_set_style_radius(cp_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(cp_dots[i], C_BG, 0);
        lv_obj_set_style_bg_opa(cp_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(cp_dots[i], C_BORDER, 0);
        lv_obj_set_style_border_width(cp_dots[i], 2, 0);
        lv_obj_set_style_pad_all(cp_dots[i], 0, 0);
    }

    lv_obj_t* err_lbl = lv_label_create(scr);
    lv_label_set_text(err_lbl, "");
    lv_obj_set_style_text_color(err_lbl, C_DANGER, 0);
    lv_obj_set_style_text_font(err_lbl, FONT_SM, 0);
    lv_obj_align(err_lbl, LV_ALIGN_TOP_MID, 0, dot_y + dot_d + 6);

    int key_d = 43, gap = 8;
    int gw = 3*key_d + 2*gap;
    int gx = (UI_W - gw) / 2;
    int gy = 120;

    struct CPCtx { lv_obj_t* title; lv_obj_t* err; uint8_t* step; char* input; int* len; char* nw; };
    static CPCtx cp_ctx;
    cp_ctx = {t2, err_lbl, &_cp_step, _cp_input, &_cp_len, _cp_new};

    static const char* kstr[12] = {"1","2","3","4","5","6","7","8","9","","0","ok"};

    for (int i = 0; i < 12; i++) {
        if (i == 9) continue;
        int col = i % 3, row_i = i / 3;
        bool is_ok = (i == 11);
        lv_obj_t* btn = lv_btn_create(scr);
        _press_fb(btn);
        lv_obj_set_size(btn, key_d, key_d);
        lv_obj_set_pos(btn, gx + col*(key_d+gap), gy + row_i*(key_d+gap));
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, C_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, C_ACCENT, LV_STATE_PRESSED);

        lv_obj_t* lbl = lv_label_create(btn);
        if (is_ok) {
            lv_label_set_text(lbl, ICO_CHECK);
            lv_obj_set_style_text_color(lbl, C_ACCENT, 0);
            lv_obj_set_style_text_font(lbl, FONT_ICON, 0);
        } else {
            lv_label_set_text(lbl, kstr[i]);
            lv_obj_set_style_text_color(lbl, C_TEXT, 0);
            lv_obj_set_style_text_font(lbl, FONT_HDR_LG, 0);
        }
        lv_obj_center(lbl);

        static const char* cp_kvals[12];
        cp_kvals[i] = kstr[i];
        lv_obj_add_event_cb(btn, [](lv_event_t* e){
            const char* k = (const char*)lv_event_get_user_data(e);
            CPCtx* c = &cp_ctx;
            if (strcmp(k, "ok") == 0) {
                if (*(c->len) < PIN_DIGITS) return;
                if (*(c->step) == 0) {
                    if (!vault_verify_pin(c->input)) {
                        memset(c->input, 0, PIN_DIGITS + 1); *(c->len) = 0;
                        lv_label_set_text(c->err, t(K_PIN_INCORRECTO));
                        for (int j=0;j<PIN_DIGITS;j++) {
                            lv_obj_set_style_bg_color(cp_dots[j], C_BG, 0);
                            lv_obj_set_style_border_color(cp_dots[j], C_BORDER, 0);
                        }
                        return;
                    }
                    *(c->step) = 1; memset(c->input, 0, PIN_DIGITS + 1); *(c->len) = 0;
                    lv_label_set_text(c->title, t(K_INGRESA_NUEVO_PIN));
                    lv_label_set_text(c->err, "");
                } else if (*(c->step) == 1) {
                    strncpy(c->nw, c->input, PIN_DIGITS);
                    *(c->step) = 2; memset(c->input, 0, PIN_DIGITS + 1); *(c->len) = 0;
                    lv_label_set_text(c->title, t(K_CONFIRMA_NUEVO_PIN));
                    lv_label_set_text(c->err, "");
                } else {
                    if (strcmp(c->nw, c->input) != 0) {
                        *(c->step) = 1; memset(c->input, 0, PIN_DIGITS + 1); *(c->len) = 0; memset(c->nw, 0, PIN_DIGITS + 1);
                        lv_label_set_text(c->title, t(K_INGRESA_NUEVO_PIN));
                        lv_label_set_text(c->err, t(K_PINS_NO_COINCIDEN));
                    } else {
                        vault_change_pin(c->nw);
                        _show_settings();
                        return;
                    }
                }
            } else {
                if (*(c->len) < PIN_DIGITS) {
                    c->input[*(c->len)] = k[0]; *(c->len) += 1; c->input[*(c->len)] = 0;
                    lv_label_set_text(c->err, "");
                }
            }
            for (int j=0;j<PIN_DIGITS;j++) {
                if (j < *(c->len)) {
                    lv_obj_set_style_bg_color(cp_dots[j], C_ACCENT, 0);
                    lv_obj_set_style_border_color(cp_dots[j], C_ACCENT, 0);
                } else {
                    lv_obj_set_style_bg_color(cp_dots[j], C_BG, 0);
                    lv_obj_set_style_border_color(cp_dots[j], C_BORDER, 0);
                }
            }
        }, LV_EVENT_CLICKED, (void*)cp_kvals[i]);
    }

    // Botón borrar
    lv_obj_t* btn_del = lv_btn_create(scr);
    lv_obj_set_size(btn_del, key_d, key_d);
    lv_obj_set_pos(btn_del, gx, gy + 3*(key_d+gap));
    lv_obj_set_style_bg_color(btn_del, C_CARD, 0);
    lv_obj_set_style_bg_opa(btn_del, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_del, 0, 0);
    lv_obj_set_style_radius(btn_del, 14, 0);
    lv_obj_set_style_shadow_width(btn_del, 0, 0);
    lv_obj_set_style_bg_color(btn_del, C_ACCENT, LV_STATE_PRESSED);
    lv_obj_t* del_lbl = lv_label_create(btn_del);
    lv_label_set_text(del_lbl, ICO_BACKSPACE);
    lv_obj_set_style_text_font(del_lbl, FONT_ICON, 0);
    lv_obj_set_style_text_color(del_lbl, C_TEXT, 0);
    lv_obj_center(del_lbl);
    _press_fb(btn_del);
    lv_obj_add_event_cb(btn_del, [](lv_event_t* e){
        CPCtx* c = &cp_ctx;
        if (*(c->len) > 0) {
            *(c->len) -= 1; c->input[*(c->len)] = 0;
            lv_label_set_text(c->err, "");
            for (int j=0;j<PIN_DIGITS;j++) {
                if (j < *(c->len)) {
                    lv_obj_set_style_bg_color(cp_dots[j], C_ACCENT, 0);
                    lv_obj_set_style_border_color(cp_dots[j], C_ACCENT, 0);
                } else {
                    lv_obj_set_style_bg_color(cp_dots[j], C_BG, 0);
                    lv_obj_set_style_border_color(cp_dots[j], C_BORDER, 0);
                }
            }
        }
    }, LV_EVENT_CLICKED, nullptr);

    // Botón volver
    lv_obj_t* btn_back = lv_btn_create(scr);
    _press_fb(btn_back);
    lv_obj_set_size(btn_back, 40, 40); lv_obj_set_pos(btn_back, 4, 4);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back);
    lv_label_set_text(back_ico, ICO_ARROW_BACK);
    lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(back_ico, C_MUTED, 0);
    lv_obj_center(back_ico);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){ kb_reset(); _show_settings(); }, LV_EVENT_CLICKED, nullptr);

    _load_screen(scr);
}


// ── PANTALLA: Información PIN señuelo (paginada) ─────────────
static int _decoy_info_page = 0;

static const int _decoy_pages[][2] = {
    { K_DECOY_P1_T, K_DECOY_P1_B },
    { K_DECOY_P2_T, K_DECOY_P2_B },
    { K_DECOY_P3_T, K_DECOY_P3_B },
    { K_DECOY_P4_T, K_DECOY_P4_B }
};
static const int _decoy_page_count = 4;

// Referencias estáticas — pantalla se crea una sola vez
static lv_obj_t* _di_scr       = nullptr;
static lv_obj_t* _di_title_lbl = nullptr;
static lv_obj_t* _di_body_lbl  = nullptr;
static lv_obj_t* _di_pg_lbl    = nullptr;
static lv_obj_t* _di_btn_prev  = nullptr;
static lv_obj_t* _di_btn_next  = nullptr;
static lv_obj_t* _di_next_lbl  = nullptr;
static lv_obj_t* _di_scroll    = nullptr;

static void _decoy_info_update() {
    if (!_di_scr || !lv_obj_is_valid(_di_scr)) return;

    // Actualizar textos
    lv_label_set_text(_di_title_lbl, t(_decoy_pages[_decoy_info_page][0]));
    lv_label_set_text(_di_body_lbl,  t(_decoy_pages[_decoy_info_page][1]));

    char pg_buf[8];
    snprintf(pg_buf, sizeof(pg_buf), "%d/%d", _decoy_info_page + 1, _decoy_page_count);
    lv_label_set_text(_di_pg_lbl, pg_buf);

    // Botón anterior
    if (_decoy_info_page > 0)
        lv_obj_clear_flag(_di_btn_prev, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(_di_btn_prev, LV_OBJ_FLAG_HIDDEN);

    // Botón siguiente — texto cambia en última página
    bool is_last = (_decoy_info_page == _decoy_page_count - 1);
    lv_label_set_text(_di_next_lbl, is_last ? t(K_HE_LEIDO_ENTIENDO) : ICO_CHEVRON_RIGHT);
    lv_obj_set_style_text_font(_di_next_lbl, is_last ? FONT_SM : FONT_ICON, 0);

    // Ancho del botón siguiente — más ancho si es última página
    if (_decoy_info_page > 0) {
        int bw = (UI_W - 40) / 2;
        int next_x = 12 + bw + 8;
        lv_obj_set_size(_di_btn_next, bw, 40);
        lv_obj_set_pos(_di_btn_next, next_x, UI_H - 52);
    } else {
        lv_obj_set_size(_di_btn_next, UI_W - 24, 40);
        lv_obj_set_pos(_di_btn_next, 12, UI_H - 52);
    }

    // Reset scroll al tope
    lv_obj_scroll_to_y(_di_scroll, 0, LV_ANIM_OFF);
}


static void _show_decoy_info() {
    _current_screen = SCREEN_DECOY_SETUP; _kb_active = false;

    // Crear pantalla solo la primera vez
    if (!_di_scr || !lv_obj_is_valid(_di_scr)) {
        lv_obj_t* scr = _make_screen();
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
        _di_scr = scr;

        // Header
        lv_obj_t* hdr = lv_obj_create(scr);
        lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
        lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
        lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* bb = lv_btn_create(hdr);
        lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
        lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(bb, 0, 0); lv_obj_set_style_border_width(bb, 0, 0);
        lv_obj_t* bi = lv_label_create(bb);
        lv_label_set_text(bi, ICO_ARROW_BACK); lv_obj_set_style_text_font(bi, FONT_ICON, 0);
        lv_obj_set_style_text_color(bi, C_TEXT, 0); lv_obj_center(bi);
        _press_fb(bb);
        lv_obj_add_event_cb(bb, [](lv_event_t* e){
            _decoy_info_page = 0;
            _di_scr = nullptr;
            _show_settings();
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* tt = lv_label_create(hdr);
        lv_label_set_text(tt, t(K_PIN_SENUELO));
        lv_obj_set_style_text_color(tt, C_TEXT, 0);
        lv_obj_set_style_text_font(tt, FONT_MD, 0);
        lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);

        _di_pg_lbl = lv_label_create(hdr);
        lv_label_set_text(_di_pg_lbl, "1/4");
        lv_obj_set_style_text_color(_di_pg_lbl, C_MUTED, 0);
        lv_obj_set_style_text_font(_di_pg_lbl, FONT_SM, 0);
        lv_obj_set_pos(_di_pg_lbl, UI_W - 36, 16);

        _divider(scr, 44);

        // Título de página
        _di_title_lbl = lv_label_create(scr);
        lv_obj_set_style_text_color(_di_title_lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(_di_title_lbl, FONT_HDR, 0);
        lv_obj_set_pos(_di_title_lbl, 16, 54);
        lv_obj_set_width(_di_title_lbl, UI_W - 32);
        lv_label_set_long_mode(_di_title_lbl, LV_LABEL_LONG_WRAP);

        // Cuerpo scrolleable — termina 70px antes del fondo
        _di_scroll = lv_obj_create(scr);
        lv_obj_set_size(_di_scroll, UI_W, UI_H - 86 - 70);
        lv_obj_set_pos(_di_scroll, 0, 86);
        lv_obj_set_style_bg_opa(_di_scroll, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(_di_scroll, 0, 0);
        lv_obj_set_style_pad_hor(_di_scroll, 16, 0);
        lv_obj_set_style_pad_ver(_di_scroll, 4, 0);
        lv_obj_set_scrollbar_mode(_di_scroll, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(_di_scroll, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(_di_scroll, LV_OBJ_FLAG_SCROLL_ELASTIC);

        _di_body_lbl = lv_label_create(_di_scroll);
        lv_obj_set_style_text_color(_di_body_lbl, C_MUTED, 0);
        lv_obj_set_style_text_font(_di_body_lbl, FONT_MD, 0);
        lv_obj_set_width(_di_body_lbl, UI_W - 32);
        lv_label_set_long_mode(_di_body_lbl, LV_LABEL_LONG_WRAP);

        // Degradado inferior — indica que hay más texto
        lv_obj_t* fade = lv_obj_create(scr);
        lv_obj_set_size(fade, UI_W, 32);
        lv_obj_set_pos(fade, 0, UI_H - 56 - 32);
        lv_obj_set_style_bg_color(fade, C_BG, 0);
        lv_obj_set_style_bg_opa(fade, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_grad_color(fade, C_BG, 0);
        lv_obj_set_style_bg_grad_dir(fade, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_main_stop(fade, 0, 0);
        lv_obj_set_style_bg_grad_stop(fade, 255, 0);
        // Transparente arriba, C_BG abajo
        lv_obj_set_style_bg_opa(fade, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(fade, 0, 0);
        lv_obj_set_style_pad_all(fade, 0, 0);
        lv_obj_clear_flag(fade, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(fade, LV_OBJ_FLAG_CLICKABLE);
        // Gradiente de transparente (arriba) a C_BG (abajo)
        lv_obj_set_style_bg_color(fade, C_BG, 0);
        lv_obj_set_style_bg_grad_color(fade, C_BG, 0);
        lv_obj_set_style_bg_grad_dir(fade, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_main_stop(fade, 0, 0);
        lv_obj_set_style_bg_grad_stop(fade, 255, 0);
        lv_obj_set_style_bg_opa(fade, LV_OPA_COVER, 0);
        lv_obj_set_style_opa(fade, LV_OPA_COVER, 0);
        // El color inicial es transparente via imagen de fondo
        lv_obj_set_style_bg_img_src(fade, nullptr, 0);

        // Ocultar degradado cuando se llega al fondo del scroll
        lv_obj_add_event_cb(_di_scroll, [](lv_event_t* e){
            lv_obj_t* fd = (lv_obj_t*)lv_event_get_user_data(e);
            if (!fd || !lv_obj_is_valid(fd)) return;
            if (lv_obj_get_scroll_bottom(_di_scroll) <= 4)
                lv_obj_add_flag(fd, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_clear_flag(fd, LV_OBJ_FLAG_HIDDEN);
        }, LV_EVENT_SCROLL, fade);

        // Divisor antes de botones
        lv_obj_t* div_btn = lv_obj_create(scr);
        lv_obj_set_size(div_btn, UI_W, 1);
        lv_obj_set_pos(div_btn, 0, UI_H - 56);
        lv_obj_set_style_bg_color(div_btn, C_BORDER, 0);
        lv_obj_set_style_bg_opa(div_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(div_btn, 0, 0);

        // Botón anterior
        int bw = (UI_W - 40) / 2;
        _di_btn_prev = lv_btn_create(scr);
        lv_obj_set_size(_di_btn_prev, bw, 40); lv_obj_set_pos(_di_btn_prev, 12, UI_H - 52);
        lv_obj_set_style_bg_color(_di_btn_prev, C_CARD, 0); lv_obj_set_style_radius(_di_btn_prev, 10, 0);
        lv_obj_set_style_shadow_width(_di_btn_prev, 0, 0); lv_obj_set_style_border_width(_di_btn_prev, 0, 0);
        _press_fb(_di_btn_prev);
        lv_obj_t* prev_lbl = lv_label_create(_di_btn_prev);
        lv_label_set_text(prev_lbl, ICO_CHEVRON_LEFT);
        lv_obj_set_style_text_color(prev_lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(prev_lbl, FONT_ICON, 0); lv_obj_center(prev_lbl);
        lv_obj_add_flag(_di_btn_prev, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(_di_btn_prev, [](lv_event_t* e){
            _decoy_info_page--;
            _decoy_info_update();
        }, LV_EVENT_CLICKED, nullptr);

        // Botón siguiente
        _di_btn_next = lv_btn_create(scr);
        lv_obj_set_style_bg_color(_di_btn_next, C_ACCENT, 0);
        lv_obj_set_style_radius(_di_btn_next, 10, 0);
        lv_obj_set_style_shadow_width(_di_btn_next, 0, 0);
        lv_obj_set_style_border_width(_di_btn_next, 0, 0);
        _press_fb(_di_btn_next);
        _di_next_lbl = lv_label_create(_di_btn_next);
        lv_obj_set_style_text_color(_di_next_lbl, C_ACCENT_TEXT, 0);
        lv_obj_center(_di_next_lbl);
        lv_obj_add_event_cb(_di_btn_next, [](lv_event_t* e){
            if (_decoy_info_page < _decoy_page_count - 1) {
                _decoy_info_page++;
                _decoy_info_update();
            } else {
                _decoy_info_page = 0;
                _di_scr = nullptr;
                _show_decoy_pin_setup();
            }
        }, LV_EVENT_CLICKED, nullptr);
    }

    // Actualizar contenido y cargar
    _decoy_info_update();
    _load_screen_instant(_di_scr);
}


// ── PANTALLA: Configurar PIN señuelo ─────────────────────────
static void _show_decoy_pin_setup() {
    _current_screen = SCREEN_DECOY_SETUP; _kb_active = false;
    static uint8_t _dp_step = 0;
    static char _dp_new[PIN_DIGITS + 1]   = {0};
    static char _dp_input[PIN_DIGITS + 1] = {0};
    static int  _dp_len      = 0;
    _dp_step = 0;
    memset(_dp_new,   0, PIN_DIGITS + 1);
    memset(_dp_input, 0, PIN_DIGITS + 1);
    _dp_len = 0;

    lv_obj_t* scr = _make_screen();

    // Ícono
    lv_obj_t* lock_ico = lv_label_create(scr);
    lv_label_set_text(lock_ico, ICO_VISIBILITY_OFF);
    lv_obj_set_style_text_font(lock_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(lock_ico, C_ACCENT, 0);
    lv_obj_align(lock_ico, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t* t2 = lv_label_create(scr);
    lv_label_set_text(t2, t(K_NUEVO_PIN_SENUELO));
    lv_obj_set_style_text_color(t2, C_TEXT, 0);
    lv_obj_set_style_text_font(t2, FONT_HDR, 0);
    lv_obj_align(t2, LV_ALIGN_TOP_MID, 0, 40);

    int dot_d = 14, dot_gap = 24;
    int dot_tot = PIN_DIGITS*dot_d + (PIN_DIGITS-1)*dot_gap;
    int dot_x0 = (UI_W - dot_tot) / 2;
    int dot_y = 88;

    static lv_obj_t* dp_dots[PIN_DIGITS];
    for (int i = 0; i < PIN_DIGITS; i++) {
        dp_dots[i] = lv_obj_create(scr);
        lv_obj_set_size(dp_dots[i], dot_d, dot_d);
        lv_obj_set_pos(dp_dots[i], dot_x0 + i*(dot_d+dot_gap), dot_y);
        lv_obj_set_style_radius(dp_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dp_dots[i], C_BG, 0);
        lv_obj_set_style_bg_opa(dp_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(dp_dots[i], C_BORDER, 0);
        lv_obj_set_style_border_width(dp_dots[i], 2, 0);
        lv_obj_set_style_pad_all(dp_dots[i], 0, 0);
    }

    lv_obj_t* err_lbl = lv_label_create(scr);
    lv_label_set_text(err_lbl, "");
    lv_obj_set_style_text_color(err_lbl, C_DANGER, 0);
    lv_obj_set_style_text_font(err_lbl, FONT_SM, 0);
    lv_obj_align(err_lbl, LV_ALIGN_TOP_MID, 0, dot_y + dot_d + 6);

    int key_d = 43, gap = 8;
    int gw = 3*key_d + 2*gap;
    int gx = (UI_W - gw) / 2;
    int gy = 120;

    struct DPCtx { lv_obj_t* t; lv_obj_t* e; uint8_t* s; char* in; int* l; char* nw; };
    static DPCtx dp_ctx;
    dp_ctx = {t2, err_lbl, &_dp_step, _dp_input, &_dp_len, _dp_new};

    static const char* dp_kvals[12];
    static const char* dp_kstr[12] = {"1","2","3","4","5","6","7","8","9","","0","ok"};
    for (int i = 0; i < 12; i++) dp_kvals[i] = dp_kstr[i];

    for (int i = 0; i < 12; i++) {
        if (i == 9) continue;
        int col = i % 3, row_i = i / 3;
        bool is_ok = (i == 11);
        lv_obj_t* btn = lv_btn_create(scr);
        _press_fb(btn);
        lv_obj_set_size(btn, key_d, key_d);
        lv_obj_set_pos(btn, gx + col*(key_d+gap), gy + row_i*(key_d+gap));
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, C_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, C_ACCENT, LV_STATE_PRESSED);

        lv_obj_t* lbl = lv_label_create(btn);
        if (is_ok) {
            lv_label_set_text(lbl, ICO_CHECK);
            lv_obj_set_style_text_color(lbl, C_ACCENT, 0);
            lv_obj_set_style_text_font(lbl, FONT_ICON, 0);
        } else {
            lv_label_set_text(lbl, dp_kstr[i]);
            lv_obj_set_style_text_color(lbl, C_TEXT, 0);
            lv_obj_set_style_text_font(lbl, FONT_HDR_LG, 0);
        }
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, [](lv_event_t* e){
            const char* k = (const char*)lv_event_get_user_data(e);
            DPCtx* c = &dp_ctx;
            if (strcmp(k, "ok") == 0) {
                if (*(c->l) < PIN_DIGITS) return;
                if (*(c->s) == 0) {
                    if (vault_verify_pin(c->in)) {
                        memset(c->in, 0, PIN_DIGITS + 1); *(c->l) = 0;
                        lv_label_set_text(c->e, t(K_NO_IGUAL_PIN_REAL));
                        for (int j=0;j<PIN_DIGITS;j++) {
                            lv_obj_set_style_bg_color(dp_dots[j], C_BG, 0);
                            lv_obj_set_style_border_color(dp_dots[j], C_BORDER, 0);
                        }
                        return;
                    }
                    strncpy(c->nw, c->in, PIN_DIGITS);
                    *(c->s) = 1; memset(c->in, 0, PIN_DIGITS + 1); *(c->l) = 0;
                    lv_label_set_text(c->t, t(K_CONFIRMA_PIN_SENUELO));
                    lv_label_set_text(c->e, "");
                } else {
                    if (strcmp(c->nw, c->in) != 0) {
                        *(c->s) = 0; memset(c->in, 0, PIN_DIGITS + 1); *(c->l) = 0; memset(c->nw, 0, PIN_DIGITS + 1);
                        lv_label_set_text(c->t, t(K_NUEVO_PIN_SENUELO));
                        lv_label_set_text(c->e, t(K_PINS_NO_COINCIDEN));
                    } else {
                        vault_decoy_set_pin(c->nw);
                        kb_reset(); _show_settings();
                        return;
                    }
                }
            } else {
                if (*(c->l) < PIN_DIGITS) {
                    c->in[*(c->l)] = k[0]; *(c->l) += 1; c->in[*(c->l)] = 0;
                    lv_label_set_text(c->e, "");
                }
            }
            for (int j=0;j<PIN_DIGITS;j++) {
                if (j < *(c->l)) {
                    lv_obj_set_style_bg_color(dp_dots[j], C_ACCENT, 0);
                    lv_obj_set_style_border_color(dp_dots[j], C_ACCENT, 0);
                } else {
                    lv_obj_set_style_bg_color(dp_dots[j], C_BG, 0);
                    lv_obj_set_style_border_color(dp_dots[j], C_BORDER, 0);
                }
            }
        }, LV_EVENT_CLICKED, (void*)dp_kvals[i]);
    }

    // Botón borrar
    lv_obj_t* btn_del = lv_btn_create(scr);
    lv_obj_set_size(btn_del, key_d, key_d);
    lv_obj_set_pos(btn_del, gx, gy + 3*(key_d+gap));
    lv_obj_set_style_bg_color(btn_del, C_CARD, 0);
    lv_obj_set_style_bg_opa(btn_del, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_del, 0, 0);
    lv_obj_set_style_radius(btn_del, 14, 0);
    lv_obj_set_style_shadow_width(btn_del, 0, 0);
    lv_obj_set_style_bg_color(btn_del, C_ACCENT, LV_STATE_PRESSED);
    lv_obj_t* del_lbl = lv_label_create(btn_del);
    lv_label_set_text(del_lbl, ICO_BACKSPACE);
    lv_obj_set_style_text_font(del_lbl, FONT_ICON, 0);
    lv_obj_set_style_text_color(del_lbl, C_TEXT, 0);
    lv_obj_center(del_lbl);
    _press_fb(btn_del);
    lv_obj_add_event_cb(btn_del, [](lv_event_t* e){
        DPCtx* c = &dp_ctx;
        if (*(c->l) > 0) {
            *(c->l) -= 1; c->in[*(c->l)] = 0;
            lv_label_set_text(c->e, "");
            for (int j=0;j<PIN_DIGITS;j++) {
                if (j < *(c->l)) {
                    lv_obj_set_style_bg_color(dp_dots[j], C_ACCENT, 0);
                    lv_obj_set_style_border_color(dp_dots[j], C_ACCENT, 0);
                } else {
                    lv_obj_set_style_bg_color(dp_dots[j], C_BG, 0);
                    lv_obj_set_style_border_color(dp_dots[j], C_BORDER, 0);
                }
            }
        }
    }, LV_EVENT_CLICKED, nullptr);

    // Botón volver
    lv_obj_t* btn_back = lv_btn_create(scr);
    _press_fb(btn_back);
    lv_obj_set_size(btn_back, 40, 40); lv_obj_set_pos(btn_back, 4, 4);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0); lv_obj_set_style_shadow_width(btn_back, 0, 0); lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back); lv_label_set_text(back_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(back_ico, C_MUTED, 0); lv_obj_set_style_text_font(back_ico, FONT_ICON, 0); lv_obj_center(back_ico);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){ _show_settings(); }, LV_EVENT_CLICKED, nullptr);

    _load_screen(scr);
}
