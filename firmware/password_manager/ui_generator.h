#pragma once
// =============================================================
//  ui_generator.h — generador de contraseñas (pantalla standalone
//  y modo embebido en formularios). Extraído de ui_main.h.
//  Depende de helpers definidos antes en ui_main.h: _make_screen,
//  _press_fb, _fade_overlay, _load_screen_instant, _show_form_web,
//  _show_form_secret, _show_list, custom_kb_reset, _current_screen,
//  _kb_active, colores C_*, fuentes FONT_*, iconos ICO_*.
// =============================================================
#include <lvgl.h>
#include "vault.h"
#include "i18n.h"

static void _show_password_generator(lv_obj_t* target_ta) {
    if (target_ta == nullptr) custom_kb_reset();
    _current_screen = SCREEN_NONE; _kb_active = false;
    static char _gen_buf[33] = {0};
    static int  _gen_len     = 10;
    static bool _gen_upper   = true;
    static bool _gen_nums    = true;
    static bool _gen_syms    = false;
    static bool _gen_ambig   = true;
    static lv_obj_t* _gen_lbl = nullptr;
    static lv_obj_t* _target  = nullptr;
    _target  = target_ta;
    _gen_lbl = nullptr;

    // Cargar configuración persistente
    VaultGenConfig cfg = vault_get_gen_config();
    _gen_len   = cfg.len;
    _gen_upper = cfg.upper;
    _gen_nums  = cfg.nums;
    _gen_syms  = cfg.syms;
    _gen_ambig = cfg.ambig;

    vault_generate_password(_gen_buf, _gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
    if (_gen_lbl) lv_label_set_text(_gen_lbl, _gen_buf);

    lv_obj_t* scr = _make_screen();
    lv_obj_add_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(scr, 16, 0);
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 36); lv_obj_set_pos(hdr, 0, 0); // la barra de estado ya no se muestra fuera de la lista
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_back = lv_btn_create(hdr);
    lv_obj_set_size(btn_back, 36, 36); lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0); lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back); lv_label_set_text(back_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(back_ico, C_TEXT, 0);
    lv_obj_set_style_text_font(back_ico, FONT_ICON, 0); lv_obj_center(back_ico);
    _press_fb(btn_back);
    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, t(K_GENERADOR));
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Botón regenerar
    lv_obj_t* btn_regen = lv_btn_create(hdr);
    lv_obj_set_size(btn_regen, 36, 36); lv_obj_set_pos(btn_regen, UI_W - 36, 0);
    lv_obj_set_style_bg_opa(btn_regen, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_regen, 0, 0); lv_obj_set_style_border_width(btn_regen, 0, 0);
    lv_obj_t* regen_ico = lv_label_create(btn_regen); lv_label_set_text(regen_ico, ICO_AUTORENEW); lv_obj_set_style_text_font(regen_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(regen_ico, C_ACCENT, 0);
    lv_obj_center(regen_ico);
    _press_fb(btn_regen);
    lv_obj_add_event_cb(btn_regen, [](lv_event_t* e){
        vault_generate_password(_gen_buf, _gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
        if (_gen_lbl) lv_label_set_text(_gen_lbl, _gen_buf);
    }, LV_EVENT_CLICKED, nullptr);

    _divider(scr, 36);

    // Display de contraseña generada
    lv_obj_t* pw_box = lv_obj_create(scr);
    lv_obj_set_size(pw_box, UI_W - 16, 42); lv_obj_set_pos(pw_box, 8, 44);
    lv_obj_set_style_bg_color(pw_box, C_CARD, 0); lv_obj_set_style_bg_opa(pw_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pw_box, 0, 0); lv_obj_set_style_radius(pw_box, 10, 0);
    lv_obj_set_style_pad_all(pw_box, 10, 0); lv_obj_clear_flag(pw_box, LV_OBJ_FLAG_SCROLLABLE);

    _gen_lbl = lv_label_create(pw_box);
    vault_generate_password(_gen_buf, _gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
    lv_label_set_text(_gen_lbl, _gen_buf);
    lv_obj_set_style_text_color(_gen_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(_gen_lbl, FONT_MD, 0);
    lv_label_set_long_mode(_gen_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(_gen_lbl, UI_W - 40);
    lv_obj_align(_gen_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    // Selector de longitud
    lv_obj_t* len_lbl = lv_label_create(scr);
    lv_label_set_text(len_lbl, t(K_LONGITUD));
    lv_obj_set_style_text_color(len_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(len_lbl, FONT_SM, 0);
    lv_obj_set_pos(len_lbl, 8, 96);

    static lv_obj_t* _len_val_lbl = nullptr;
    _len_val_lbl = lv_label_create(scr);
    char len_str[4]; snprintf(len_str, sizeof(len_str), "%d", _gen_len);
    lv_label_set_text(_len_val_lbl, len_str);
    lv_obj_set_style_text_color(_len_val_lbl, C_MUTED, 0);
    lv_obj_set_style_text_font(_len_val_lbl, FONT_SM, 0);
    lv_obj_set_pos(_len_val_lbl, UI_W - 8 - 24, 96);

    lv_obj_t* slider = lv_slider_create(scr);
    lv_obj_set_size(slider, UI_W - 80, 4);
    lv_obj_set_pos(slider, 40, 120);
    lv_slider_set_range(slider, 8, 32);
    lv_slider_set_value(slider, _gen_len, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, C_CARD, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, C_ACCENT, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, [](lv_event_t* e){
        lv_obj_t* s = (lv_obj_t*)lv_event_get_target(e);
        _gen_len = lv_slider_get_value(s);
        char buf[4]; snprintf(buf, sizeof(buf), "%d", _gen_len);
        if (_len_val_lbl) lv_label_set_text(_len_val_lbl, buf);
        vault_generate_password(_gen_buf, _gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
        if (_gen_lbl) lv_label_set_text(_gen_lbl, _gen_buf);
        vault_set_gen_config(_gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // Opciones
    int oy = 141;
    int row_h = 35;
    int card_w = UI_W - 16;
    const int N_TOGGLES = 3;

    lv_obj_t* opts_card = lv_obj_create(scr);
    lv_obj_set_size(opts_card, card_w, row_h * N_TOGGLES);
    lv_obj_set_pos(opts_card, 8, oy);
    lv_obj_set_style_bg_opa(opts_card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(opts_card, 0, 0);
    lv_obj_set_style_pad_all(opts_card, 0, 0);
    lv_obj_clear_flag(opts_card, LV_OBJ_FLAG_SCROLLABLE);

    // Helper para crear una fila DENTRO del contenedor único
    static lv_obj_t* _tog[3]   = {nullptr, nullptr, nullptr};
    static lv_obj_t* _thumb[3] = {nullptr, nullptr, nullptr};

    auto toggle_row = [&](const char* label, bool* val, int idx) -> lv_obj_t* {
        lv_obj_t* row = lv_obj_create(opts_card);
        lv_obj_set_size(row, card_w, row_h);
        lv_obj_set_pos(row, 0, idx * row_h);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0); lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(row, [](lv_event_t* e){ vibro_pulse(); }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl, FONT_MD, 0);
        lv_obj_set_pos(lbl, 12, (row_h - 16) / 2);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* tog = lv_obj_create(row);
        lv_obj_set_size(tog, 36, 20);
        lv_obj_set_pos(tog, card_w - 48, (row_h - 20) / 2);
        lv_obj_set_style_radius(tog, 10, 0);
        lv_obj_set_style_bg_color(tog, *val ? C_ACCENT : lv_color_hex(0x555558), 0);
        lv_obj_set_style_bg_opa(tog, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(tog, 0, 0);
        lv_obj_set_style_pad_all(tog, 0, 0);
        lv_obj_clear_flag(tog, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(tog, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* thumb = lv_obj_create(tog);
        lv_obj_set_size(thumb, 16, 16);
        lv_obj_set_pos(thumb, *val ? 18 : 2, 2);
        lv_obj_set_style_radius(thumb, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(thumb, C_BG, 0);
        lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(thumb, 0, 0);
        lv_obj_set_style_pad_all(thumb, 0, 0);
        lv_obj_set_style_shadow_width(thumb, 0, 0);
        lv_obj_clear_flag(thumb, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(thumb, LV_OBJ_FLAG_SCROLLABLE);

        _gen_tog[idx]   = tog;
        _gen_thumb[idx] = thumb;
        _tog[idx]   = tog;
        _thumb[idx] = thumb;

        return row;
    };

    // Mayúsculas
    lv_obj_t* row_upper = toggle_row(t(K_MAYUSCULAS), &_gen_upper, 0);
    lv_obj_add_event_cb(row_upper, [](lv_event_t* e){
        _gen_upper = !_gen_upper;
        _update_gen_toggle(0, _gen_upper);
        vault_generate_password(_gen_buf, _gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
        if (_gen_lbl) lv_label_set_text(_gen_lbl, _gen_buf);
        vault_set_gen_config(_gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
    }, LV_EVENT_CLICKED, nullptr);

    // Números
    lv_obj_t* row_nums = toggle_row(t(K_NUMEROS), &_gen_nums, 1);
    lv_obj_add_event_cb(row_nums, [](lv_event_t* e){
        _gen_nums = !_gen_nums;
        _update_gen_toggle(1, _gen_nums);
        vault_generate_password(_gen_buf, _gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
        if (_gen_lbl) lv_label_set_text(_gen_lbl, _gen_buf);
        vault_set_gen_config(_gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
    }, LV_EVENT_CLICKED, nullptr);

    // Símbolos
    lv_obj_t* row_syms = toggle_row(t(K_SIMBOLOS), &_gen_syms, 2);
    lv_obj_add_event_cb(row_syms, [](lv_event_t* e){
        _gen_syms = !_gen_syms;
        _update_gen_toggle(2, _gen_syms);
        vault_generate_password(_gen_buf, _gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
        if (_gen_lbl) lv_label_set_text(_gen_lbl, _gen_buf);
        vault_set_gen_config(_gen_len, _gen_upper, _gen_nums, _gen_syms, _gen_ambig);
    }, LV_EVENT_CLICKED, nullptr);

    // Botón usar contraseña — con margen después de los toggles
    int btn_use_y = oy + row_h * N_TOGGLES + 12;
    lv_obj_t* btn_use = lv_btn_create(scr);
    lv_obj_set_size(btn_use, UI_W - 16, 35); lv_obj_set_pos(btn_use, 8, btn_use_y);
    lv_obj_set_style_bg_color(btn_use, C_ACCENT, 0); lv_obj_set_style_bg_opa(btn_use, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_use, 0, 0); lv_obj_set_style_radius(btn_use, 10, 0);
    lv_obj_set_style_shadow_width(btn_use, 0, 0);
    _press_fb(btn_use);
    lv_obj_t* use_lbl = lv_label_create(btn_use);
    lv_label_set_text(use_lbl, t(K_USAR_PASSWORD));
    lv_obj_set_style_text_color(use_lbl, C_ACCENT_TEXT, 0);
    lv_obj_set_style_text_font(use_lbl, FONT_MD, 0);
    lv_obj_center(use_lbl);
    lv_obj_add_event_cb(btn_use, [](lv_event_t* e){
        if (_target != nullptr) {
            strncpy(_fw_pass_buf, _gen_buf, VAULT_FIELD_LEN-1);
            _fw_pass_visible = false;
            _show_form_web(_fw_editing);
            return;
        }
        // Sheet idéntico al del FAB
        lv_obj_t* top = lv_layer_top();
        lv_obj_t* overlay = lv_obj_create(top);
        lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(overlay, 0, 0);
        lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
        _fade_overlay(overlay, LV_OPA_50);
        lv_obj_add_event_cb(overlay, [](lv_event_t* ev){
            lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
        }, LV_EVENT_CLICKED, overlay);

        const int SH = 192;
        lv_obj_t* sheet = lv_obj_create(overlay);
        lv_obj_set_size(sheet, UI_W, SH); lv_obj_set_pos(sheet, 0, UI_H - SH);
        lv_obj_set_style_bg_color(sheet, C_BG, 0); lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sheet, 0, 0); lv_obj_set_style_radius(sheet, 12, 0);
        lv_obj_set_style_pad_all(sheet, 0, 0); lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(sheet, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

        // Título igual al FAB
        lv_obj_t* sh_title = lv_label_create(sheet);
        lv_label_set_text(sh_title, t(K_NUEVA_ENTRADA));
        lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
        lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
        lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);
        _divider(sheet, 40);

        // Cuenta
        lv_obj_t* row_web = lv_obj_create(sheet);
        lv_obj_set_size(row_web, UI_W, 44); lv_obj_set_pos(row_web, 0, 46);
        lv_obj_set_style_bg_opa(row_web, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row_web, 0, 0);
        lv_obj_set_style_pad_all(row_web, 0, 0); lv_obj_set_style_radius(row_web, 0, 0);
        lv_obj_clear_flag(row_web, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ico_web = lv_label_create(row_web);
        lv_label_set_text(ico_web, ICO_LOCK); lv_obj_set_style_text_font(ico_web, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico_web, C_TEXT, 0); lv_obj_set_pos(ico_web, 16, 14);
        lv_obj_clear_flag(ico_web, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* lbl_web = lv_label_create(row_web);
        lv_label_set_text(lbl_web, t(K_CUENTA));
        lv_obj_set_style_text_color(lbl_web, C_TEXT, 0); lv_obj_set_style_text_font(lbl_web, FONT_MD, 0);
        lv_obj_set_pos(lbl_web, 42, 14); lv_obj_clear_flag(lbl_web, LV_OBJ_FLAG_CLICKABLE);
        _press_fb(row_web);
        lv_obj_add_event_cb(row_web, [](lv_event_t* ev){
            lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
            memset(_fw_name_buf, 0, sizeof(_fw_name_buf));
            memset(_fw_user_buf, 0, sizeof(_fw_user_buf));
            strncpy(_fw_pass_buf, _gen_buf, VAULT_FIELD_LEN-1);
            _fw_pass_visible = true;
            custom_kb_reset(); _show_form_web(false);
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* div_sh = lv_obj_create(sheet);
        lv_obj_set_size(div_sh, UI_W - 32, 1); lv_obj_set_pos(div_sh, 16, 90);
        lv_obj_set_style_bg_color(div_sh, C_BORDER, 0); lv_obj_set_style_bg_opa(div_sh, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(div_sh, 0, 0); lv_obj_set_style_pad_all(div_sh, 0, 0);

        // Objeto
        lv_obj_t* row_sec = lv_obj_create(sheet);
        lv_obj_set_size(row_sec, UI_W, 44); lv_obj_set_pos(row_sec, 0, 92);
        lv_obj_set_style_bg_opa(row_sec, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row_sec, 0, 0);
        lv_obj_set_style_pad_all(row_sec, 0, 0); lv_obj_set_style_radius(row_sec, 0, 0);
        lv_obj_clear_flag(row_sec, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ico_sec = lv_label_create(row_sec);
        lv_label_set_text(ico_sec, ICO_PASSWORD); lv_obj_set_style_text_font(ico_sec, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico_sec, C_TEXT, 0); lv_obj_set_pos(ico_sec, 16, 14);
        lv_obj_clear_flag(ico_sec, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* lbl_sec = lv_label_create(row_sec);
        lv_label_set_text(lbl_sec, t(K_OBJETO));
        lv_obj_set_style_text_color(lbl_sec, C_TEXT, 0); lv_obj_set_style_text_font(lbl_sec, FONT_MD, 0);
        lv_obj_set_pos(lbl_sec, 42, 14); lv_obj_clear_flag(lbl_sec, LV_OBJ_FLAG_CLICKABLE);
        _press_fb(row_sec);
        lv_obj_add_event_cb(row_sec, [](lv_event_t* ev){
            lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
            memset(_fs_name_buf, 0, sizeof(_fs_name_buf));
            strncpy(_fs_secret_buf, _gen_buf, VAULT_FIELD_LEN-1);
            custom_kb_reset(); _show_form_secret(false);
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* div_sh2 = lv_obj_create(sheet);
        lv_obj_set_size(div_sh2, UI_W - 32, 1); lv_obj_set_pos(div_sh2, 16, 136);
        lv_obj_set_style_bg_color(div_sh2, C_BORDER, 0); lv_obj_set_style_bg_opa(div_sh2, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(div_sh2, 0, 0); lv_obj_set_style_pad_all(div_sh2, 0, 0);

        // Escribir como teclado
        lv_obj_t* row_kb = lv_obj_create(sheet);
        lv_obj_set_size(row_kb, UI_W, 44); lv_obj_set_pos(row_kb, 0, 138);
        lv_obj_set_style_bg_opa(row_kb, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row_kb, 0, 0);
        lv_obj_set_style_pad_all(row_kb, 0, 0); lv_obj_set_style_radius(row_kb, 0, 0);
        lv_obj_clear_flag(row_kb, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ico_kb = lv_label_create(row_kb);
        lv_label_set_text(ico_kb, ICO_KEYBOARD); lv_obj_set_style_text_font(ico_kb, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico_kb, C_TEXT, 0); lv_obj_set_pos(ico_kb, 16, 14);
        lv_obj_clear_flag(ico_kb, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* lbl_kb = lv_label_create(row_kb);
        lv_label_set_text(lbl_kb, t(K_ESCRIBIR_TECLADO));
        lv_obj_set_style_text_color(lbl_kb, C_TEXT, 0); lv_obj_set_style_text_font(lbl_kb, FONT_MD, 0);
        lv_obj_set_pos(lbl_kb, 42, 14); lv_obj_clear_flag(lbl_kb, LV_OBJ_FLAG_CLICKABLE);
        _press_fb(row_kb);
        lv_obj_add_event_cb(row_kb, [](lv_event_t* ev){
            lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
            kb_type_string(_gen_buf);
        }, LV_EVENT_CLICKED, nullptr);

        lv_anim_t ash;
        lv_anim_init(&ash); lv_anim_set_var(&ash, sheet);
        lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&ash, UI_H, UI_H - SH);
        lv_anim_set_time(&ash, 220); lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
        lv_anim_start(&ash);
    }, LV_EVENT_CLICKED, nullptr);

    // Actualizar botón volver
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){
        if (_target != nullptr) _show_form_web(_fw_editing);
        else                    { custom_kb_reset(); _show_list(); }
    }, LV_EVENT_CLICKED, nullptr);

    _load_screen_instant(scr);
}
