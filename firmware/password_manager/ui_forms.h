#pragma once
// =============================================================
//  ui_forms.h — flujo de alta/edición de entradas: elegir tipo,
//  selector de categoría, formulario web, formulario secreto,
//  editor de campo en pantalla completa. Extraído de ui_main.h.
//  Depende de helpers/colores/fuentes/globales (_fw_*/_fs_*,
//  _show_field_editor, _fed_ctx) definidos antes en ui_main.h.
// =============================================================
#include <lvgl.h>
#include "vault.h"
#include "i18n.h"

// ── Popup: confirmar descartar cambios sin guardar ─────────────
static void _show_discard_confirm(void (*on_discard)()) {
    lv_obj_t* overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, UI_W, UI_H);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(overlay, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

    const int CW = UI_W - 48, CH = 150;
    lv_obj_t* card = lv_obj_create(overlay);
    lv_obj_set_size(card, CW, CH);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, t(K_DESCARTAR_CAMBIOS));
    lv_obj_set_style_text_font(title, FONT_HDR, 0);
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_width(title, CW - 32);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 0);

    lv_obj_t* desc = lv_label_create(card);
    lv_label_set_text(desc, t(K_CAMBIOS_SIN_GUARDAR_DESC));
    lv_obj_set_size(desc, CW - 32, 40);
    lv_obj_set_pos(desc, 0, 30);
    lv_obj_set_style_text_font(desc, FONT_SM, 0);
    lv_obj_set_style_text_color(desc, C_MUTED, 0);
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);

    int bw = (CW - 32 - 12) / 2;
    lv_obj_t* btn_cancel = lv_obj_create(card);
    lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn_cancel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_cancel, bw, 40);
    lv_obj_set_pos(btn_cancel, 0, 84);
    lv_obj_set_style_bg_color(btn_cancel, C_BG, 0);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 10, 0);
    lv_obj_t* cancel_lbl = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_lbl, t(K_CANCELAR));
    lv_obj_set_style_text_color(cancel_lbl, C_MUTED, 0);
    lv_obj_set_style_text_font(cancel_lbl, FONT_MD, 0);
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* ev){
        lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
    }, LV_EVENT_CLICKED, nullptr);

    static void (*_discard_cb)() = nullptr;
    _discard_cb = on_discard;
    lv_obj_t* btn_ok = lv_obj_create(card);
    lv_obj_add_flag(btn_ok, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn_ok, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_ok, bw, 40);
    lv_obj_set_pos(btn_ok, bw + 12, 84);
    lv_obj_set_style_bg_color(btn_ok, C_DANGER, 0);
    lv_obj_set_style_bg_opa(btn_ok, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 10, 0);
    lv_obj_t* ok_lbl = lv_label_create(btn_ok);
    lv_label_set_text(ok_lbl, t(K_DESCARTAR));
    lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ok_lbl, FONT_MD, 0);
    lv_obj_center(ok_lbl);
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* ev){
        lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
        if (_discard_cb) _discard_cb();
    }, LV_EVENT_CLICKED, nullptr);
}

// ── PANTALLA: Elegir tipo ─────────────────────────────────────
static void _show_pick_type() {
    lv_obj_t* scr = _make_screen();

    // Botón atrás
    lv_obj_t* btn_b = lv_btn_create(scr);
    lv_obj_set_size(btn_b, 36, 36);
    lv_obj_set_pos(btn_b, 8, 8);
    lv_obj_set_style_bg_opa(btn_b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_b, 0, 0);
    lv_obj_set_style_border_width(btn_b, 0, 0);
    lv_obj_t* b_ico = lv_label_create(btn_b);
    lv_label_set_text(b_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(b_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(b_ico, C_TEXT, 0);
    lv_obj_set_style_text_font(b_ico, FONT_ICON, 0);
    lv_obj_center(b_ico);
    _press_fb(btn_b);
    lv_obj_add_event_cb(btn_b, [](lv_event_t* e){ _show_list(); },
                        LV_EVENT_CLICKED, nullptr);

    // Título centrado
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, t(K_TIPO_ENTRADA));
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    _divider(scr, 40);

    // Fila estilo lista — igual que entradas en pantalla principal
    // Sin fondo sólido, solo borde inferior, flecha centrada verticalmente

    // Fila: Cuenta web
    lv_obj_t* row_w = lv_obj_create(scr);
    lv_obj_set_size(row_w, UI_W, 64);
    lv_obj_set_pos(row_w, 0, 48);
    lv_obj_set_style_bg_opa(row_w, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_w, 0, 0);
    lv_obj_set_style_pad_all(row_w, 0, 0);
    lv_obj_set_style_radius(row_w, 0, 0);
    lv_obj_clear_flag(row_w, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* w_title = lv_label_create(row_w);
    lv_label_set_text(w_title, t(K_CUENTA_WEB));
    lv_obj_set_style_text_color(w_title, C_TEXT, 0);
    lv_obj_set_style_text_font(w_title, FONT_MD, 0);
    lv_obj_set_pos(w_title, 16, 12);

    lv_obj_t* w_sub = lv_label_create(row_w);
    lv_label_set_text(w_sub, t(K_USUARIO_PASSWORD_USB));
    lv_obj_set_style_text_color(w_sub, C_MUTED, 0);
    lv_obj_set_style_text_font(w_sub, FONT_SM, 0);
    lv_obj_set_pos(w_sub, 16, 32);

    lv_obj_t* w_arr = lv_label_create(row_w);
    lv_label_set_text(w_arr, ICO_CHEVRON_RIGHT); lv_obj_set_style_text_font(w_arr, FONT_ICON, 0);
    lv_obj_set_style_text_color(w_arr, C_TEXT, 0);
    lv_obj_set_style_text_font(w_arr, FONT_ICON, 0);
    lv_obj_align(w_arr, LV_ALIGN_RIGHT_MID, -12, 0);

    lv_obj_add_event_cb(row_w, [](lv_event_t* e){
        memset(_fw_name_buf, 0, sizeof(_fw_name_buf));
        memset(_fw_user_buf, 0, sizeof(_fw_user_buf));
        memset(_fw_pass_buf, 0, sizeof(_fw_pass_buf));
        _show_form_web(false);
    }, LV_EVENT_CLICKED, nullptr);

    // Fila: Secreto
    lv_obj_t* row_s = lv_obj_create(scr);
    lv_obj_set_size(row_s, UI_W, 64);
    lv_obj_set_pos(row_s, 0, 112);
    lv_obj_set_style_bg_opa(row_s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_s, 0, 0);
    lv_obj_set_style_pad_all(row_s, 0, 0);
    lv_obj_set_style_radius(row_s, 0, 0);
    lv_obj_clear_flag(row_s, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* s_title = lv_label_create(row_s);
    lv_label_set_text(s_title, t(K_SECRETO));
    lv_obj_set_style_text_color(s_title, C_TEXT, 0);
    lv_obj_set_style_text_font(s_title, FONT_MD, 0);
    lv_obj_set_pos(s_title, 16, 12);

    lv_obj_t* s_sub = lv_label_create(row_s);
    lv_label_set_text(s_sub, t(K_PIN_CODIGO_TARJETA));
    lv_obj_set_style_text_color(s_sub, C_MUTED, 0);
    lv_obj_set_style_text_font(s_sub, FONT_SM, 0);
    lv_obj_set_pos(s_sub, 16, 32);

    lv_obj_t* s_arr = lv_label_create(row_s);
    lv_label_set_text(s_arr, ICO_CHEVRON_RIGHT); lv_obj_set_style_text_font(s_arr, FONT_ICON, 0);
    lv_obj_set_style_text_color(s_arr, C_TEXT, 0);
    lv_obj_set_style_text_font(s_arr, FONT_ICON, 0);
    lv_obj_align(s_arr, LV_ALIGN_RIGHT_MID, -12, 0);

    lv_obj_add_event_cb(row_s, [](lv_event_t* e){
        memset(_fs_name_buf, 0, sizeof(_fs_name_buf));
        memset(_fs_secret_buf, 0, sizeof(_fs_secret_buf));
        _show_form_secret(false);
    }, LV_EVENT_CLICKED, nullptr);

    _load_screen(scr);
}

// ── Fila de campo en formulario ───────────────────────────────
static void _form_row(lv_obj_t* scr, const char* label,
                       const char* value, const char* placeholder,
                       bool is_pass, int x, int y, int fw,
                       int return_form, int field_idx) {
    lv_obj_t* cap = lv_label_create(scr);
    lv_label_set_text(cap, label);
    lv_obj_set_style_text_color(cap, C_MUTED, 0);
    lv_obj_set_style_text_font(cap, FONT_SM, 0);
    lv_obj_set_pos(cap, x, y);

    bool has_val = value && strlen(value) > 0;
    lv_obj_t* val_lbl = lv_label_create(scr);
    if (has_val) {
        lv_label_set_text(val_lbl, is_pass ? "••••••••••••" : value);
        lv_obj_set_style_text_color(val_lbl, C_TEXT, 0);
    } else {
        lv_label_set_text(val_lbl, placeholder);
        lv_obj_set_style_text_color(val_lbl, C_MUTED, 0);
    }
    lv_obj_set_style_text_font(val_lbl, FONT_MD, 0);
    lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(val_lbl, fw - 32);
    lv_obj_set_pos(val_lbl, x, y + 14);

    // Botón editar
    lv_obj_t* btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 28, 28);
    lv_obj_set_pos(btn, x + fw - 28, y + 10);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_t* btn_ico = lv_label_create(btn);
    lv_label_set_text(btn_ico, ICO_EDIT); lv_obj_set_style_text_font(btn_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(btn_ico, C_MUTED, 0);
    lv_obj_set_style_text_font(btn_ico, FONT_ICON, 0);
    lv_obj_center(btn_ico);

    struct FD { char lbl[32]; char val[VAULT_FIELD_LEN]; bool pw; int rf; int fi; };
    static FD fd[6];
    int fi = field_idx < 6 ? field_idx : 0;
    strncpy(fd[fi].lbl, label, 31);
    strncpy(fd[fi].val, value ? value : "", VAULT_FIELD_LEN-1);
    fd[fi].pw = is_pass;
    fd[fi].rf = return_form;
    fd[fi].fi = field_idx;

    lv_obj_add_event_cb(btn, [](lv_event_t* e){
        FD* d = (FD*)lv_event_get_user_data(e);
        _show_keyboard_fullscreen(d->lbl, d->val, d->pw, d->rf, d->fi);
    }, LV_EVENT_CLICKED, &fd[fi]);

    _divider(scr, y + 36);
}

// ── PANTALLA: Formulario web ──────────────────────────────────

// ── Helper: selector de categoría (bottom sheet) ─────────────
struct CatSelData { char id[VAULT_CAT_LEN]; char name[VAULT_CAT_LEN]; bool is_web; };
static CatSelData _cat_sel_opts[VAULT_MAX_CATS + 6];
static int _cat_sel_count = 0;

static void _show_cat_selector(bool is_web, lv_obj_t* cat_label);

static void _show_cat_selector(bool is_web, lv_obj_t* cat_label) {
    lv_obj_t* scr_act = lv_scr_act();

    lv_obj_t* ov = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ov, UI_W, UI_H);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    _fade_overlay(ov, LV_OPA_50);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ov, [](lv_event_t* ev){
        lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
    }, LV_EVENT_CLICKED, ov);

    // Construir lista de categorías desde vault únicamente
    VaultCategory custom[VAULT_MAX_CATS + 6];
    int cc = 0;
    vault_cat_list(custom, &cc, VAULT_MAX_CATS + 6);

    _cat_sel_count = 0;
    for (int i = 0; i < cc; i++) {
        strncpy(_cat_sel_opts[_cat_sel_count].id,   custom[i].id,   VAULT_CAT_LEN-1);
        strncpy(_cat_sel_opts[_cat_sel_count].name, _cat_display_name(custom[i]), VAULT_CAT_LEN-1);
        _cat_sel_opts[_cat_sel_count].is_web = is_web;
        _cat_sel_count++;
    }

    int SH = min(UI_H - 44, 48 + _cat_sel_count * 44);

    lv_obj_t* sheet = lv_obj_create(ov);
    lv_obj_set_size(sheet, UI_W, SH);
    lv_obj_set_pos(sheet, 0, UI_H - SH);
    lv_obj_set_style_bg_color(sheet, C_BG, 0);
    lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sheet, 0, 0);
    lv_obj_set_style_radius(sheet, 12, 0);
    lv_obj_set_style_pad_all(sheet, 0, 0);
    lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_add_event_cb(sheet, [](lv_event_t* ev){
        lv_event_stop_bubbling(ev);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* hndl = lv_obj_create(sheet);
    lv_obj_set_size(hndl, 40, 4);
    lv_obj_align(hndl, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(hndl, C_BORDER, 0);
    lv_obj_set_style_bg_opa(hndl, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hndl, 0, 0);
    lv_obj_set_style_radius(hndl, 2, 0);

    lv_obj_t* shtl = lv_label_create(sheet);
    lv_label_set_text(shtl, t(K_SELECCIONAR_CARPETA));
    lv_obj_set_style_text_color(shtl, C_TEXT, 0);
    lv_obj_set_style_text_font(shtl, FONT_MD, 0);
    lv_obj_align(shtl, LV_ALIGN_TOP_MID, 0, 18);
    _divider(sheet, 38);

    // Arrays de referencias para actualización inmediata del check
    static lv_obj_t* _cat_row_lbls[VAULT_MAX_CATS + 6];
    static lv_obj_t* _cat_row_chks[VAULT_MAX_CATS + 6];

    for (int i = 0; i < _cat_sel_count; i++) {
        lv_obj_t* row = lv_obj_create(sheet);
        lv_obj_set_size(row, UI_W, 40);
        lv_obj_set_pos(row, 0, 46 + i * 40);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        const char* cur = _cat_sel_opts[i].is_web ? _fw_cat_buf : _fs_cat_buf;
        bool sel = (strcmp(cur, _cat_sel_opts[i].id) == 0);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, _cat_sel_opts[i].name);
        lv_obj_set_style_text_font(lbl, FONT_SM, 0);
        lv_obj_set_style_text_color(lbl, sel ? C_BTN_BG : C_TEXT, 0);
        lv_obj_set_pos(lbl, 16, 12);
        _cat_row_lbls[i] = lbl;

        lv_obj_t* chk = lv_label_create(row);
        lv_label_set_text(chk, ICO_CHECK); lv_obj_set_style_text_font(chk, FONT_ICON, 0);
        lv_obj_set_style_text_color(chk, C_BTN_BG, 0);
        lv_obj_set_style_text_font(chk, FONT_ICON, 0);
        lv_obj_set_pos(chk, UI_W - 28, 12);
        if (!sel) lv_obj_add_flag(chk, LV_OBJ_FLAG_HIDDEN);
        _cat_row_chks[i] = chk;

        // Pasar índice, label campo, refs de filas al callback
        struct SelCtx { int idx; lv_obj_t* lbl; lv_obj_t* sheet_ref; lv_obj_t* ov_ref; };
        static SelCtx sel_ctxs[VAULT_MAX_CATS + 6];
        sel_ctxs[i] = {i, cat_label, sheet, ov};
        lv_obj_add_event_cb(row, [](lv_event_t* ev){
            SelCtx* c = (SelCtx*)lv_event_get_user_data(ev);
            int idx = c->idx;
            if (_cat_sel_opts[idx].is_web) {
                strncpy(_fw_cat_buf, _cat_sel_opts[idx].id, VAULT_CAT_LEN-1);
            } else {
                strncpy(_fs_cat_buf, _cat_sel_opts[idx].id, VAULT_CAT_LEN-1);
            }
            // Actualizar label del campo en el formulario
            if (c->lbl) {
                lv_label_set_text(c->lbl, _cat_sel_opts[idx].name);
                lv_obj_set_style_text_color(c->lbl, C_TEXT, 0);
            }
            // Actualizar checks en tiempo real usando referencias directas
            for (int j = 0; j < _cat_sel_count; j++) {
                bool is_sel = (j == idx);
                lv_obj_set_style_text_color(_cat_row_lbls[j],
                    is_sel ? C_BTN_BG : C_TEXT, 0);
                if (is_sel) lv_obj_clear_flag(_cat_row_chks[j], LV_OBJ_FLAG_HIDDEN);
                else        lv_obj_add_flag(_cat_row_chks[j],   LV_OBJ_FLAG_HIDDEN);
            }
        }, LV_EVENT_CLICKED, &sel_ctxs[i]);
    }

    lv_anim_t ash;
    lv_anim_init(&ash);
    lv_anim_set_var(&ash, sheet);
    lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&ash, UI_H, UI_H - SH);
    lv_anim_set_time(&ash, 220);
    lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
    lv_anim_start(&ash);
}static void _show_form_web(bool editing, bool instant) {
    _current_screen = SCREEN_FORM_WEB; _kb_active = false;
    _fw_editing = editing;
    if (editing) {
        VaultEntry ex = vault_load(_selected_name);
        if (ex.valid) {
            strncpy(_fw_name_buf,  ex.name,        VAULT_FIELD_LEN-1);
            strncpy(_fw_user_buf,  ex.username,    VAULT_FIELD_LEN-1);
            strncpy(_fw_pass_buf,  ex.password,    VAULT_FIELD_LEN-1);
            strncpy(_fw_cat_buf,   ex.category_id, VAULT_CAT_LEN-1);
            strncpy(_fw_notes_buf, ex.notes,       VAULT_NOTES_LEN-1);
        }
    }

    // Snapshot para detectar cambios sin guardar al presionar "atrás"
    static char _fw_base_name[VAULT_FIELD_LEN], _fw_base_user[VAULT_FIELD_LEN],
                _fw_base_pass[VAULT_FIELD_LEN], _fw_base_notes[VAULT_NOTES_LEN];
    strncpy(_fw_base_name,  _fw_name_buf,  VAULT_FIELD_LEN-1);
    strncpy(_fw_base_user,  _fw_user_buf,  VAULT_FIELD_LEN-1);
    strncpy(_fw_base_pass,  _fw_pass_buf,  VAULT_FIELD_LEN-1);
    strncpy(_fw_base_notes, _fw_notes_buf, VAULT_NOTES_LEN-1);

    _fw_kb    = nullptr;
    _fw_btn_x = nullptr;

    lv_obj_t* scr = _make_screen();

    // ── Header ────────────────────────────────────────────────
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_b = lv_btn_create(hdr);
    lv_obj_set_size(btn_b, 40, 44);
    lv_obj_set_pos(btn_b, 0, 0);
    lv_obj_set_style_bg_opa(btn_b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_b, 0, 0);
    lv_obj_set_style_border_width(btn_b, 0, 0);
    lv_obj_t* b_ico = lv_label_create(btn_b);
    lv_label_set_text(b_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(b_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(b_ico, C_TEXT, 0);
    lv_obj_set_style_text_font(b_ico, FONT_ICON, 0);
    lv_obj_center(b_ico);
    _press_fb(btn_b);
    lv_obj_add_event_cb(btn_b, [](lv_event_t* e){
        bool dirty = strcmp(_fw_name_buf,  _fw_base_name)  != 0 ||
                     strcmp(_fw_user_buf,  _fw_base_user)  != 0 ||
                     strcmp(_fw_pass_buf,  _fw_base_pass)  != 0 ||
                     strcmp(_fw_notes_buf, _fw_base_notes) != 0;
        auto discard = []{
            memset(_fw_name_buf, 0, sizeof(_fw_name_buf));
            memset(_fw_user_buf, 0, sizeof(_fw_user_buf));
            memset(_fw_pass_buf, 0, sizeof(_fw_pass_buf));
            memset(_fw_notes_buf, 0, sizeof(_fw_notes_buf));
            _fw_editing ? _show_detail(_selected_name) : _show_list();
        };
        if (dirty) _show_discard_confirm(discard);
        else       discard();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* hdr_title = lv_label_create(hdr);
    lv_label_set_text(hdr_title, editing ? t(K_EDITAR_ENTRADA) : t(K_ANADIR_ENTRADA));
    lv_obj_set_style_text_color(hdr_title, C_TEXT, 0);
    lv_obj_set_style_text_font(hdr_title, FONT_MD, 0);
    lv_obj_align(hdr_title, LV_ALIGN_CENTER, 0, 0);

    // Sin btn_x — el teclado se maneja en el editor dedicado

    _divider(scr, 44);

    // ── Body scrolleable — TODOS los widgets van aquí ─────────
    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_W, UI_H - 44);
    lv_obj_set_pos(body, 0, 44);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0);
    lv_obj_set_style_pad_top(body, 14, 0);
    lv_obj_set_style_pad_bottom(body, 24, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, 4, 0);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    int fw = UI_W - 32;
    int fh = 36;  // altura uniforme para todos los campos

    // Helper: label + textarea todo dentro de body
    auto make_field = [&](const char* cap, const char* ph,
                           const char* pre, bool pw) -> lv_obj_t* {

        lv_obj_t* lbl = lv_label_create(body);
        lv_label_set_text(lbl, cap);
        lv_obj_set_style_text_color(lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl, FONT_MD, 0);
        lv_obj_set_size(lbl, fw, 16);
        lv_obj_set_style_min_height(lbl, 16, 0);
        lv_obj_set_style_max_height(lbl, 16, 0);
        lv_obj_set_style_pad_all(lbl, 0, 0);

        lv_obj_t* ta = lv_textarea_create(body);
        lv_obj_set_size(ta, fw, fh);
        lv_obj_set_style_min_height(ta, fh, 0);
        lv_obj_set_style_max_height(ta, fh, 0);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_max_length(ta, VAULT_FIELD_LEN - 1);
        lv_textarea_set_placeholder_text(ta, ph);
        if (pw) lv_textarea_set_password_mode(ta, true);
        if (pre && strlen(pre) > 0) lv_textarea_set_text(ta, pre);
        lv_obj_set_style_bg_color(ta, C_CARD, 0);
        lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(ta, C_BORDER, 0);
        lv_obj_set_style_border_width(ta, 1, 0);
        lv_obj_set_style_radius(ta, 10, 0);
        lv_obj_set_style_text_color(ta, C_TEXT, 0);
        lv_obj_set_style_text_font(ta, FONT_SM, 0);
        lv_obj_set_style_pad_top(ta, (fh - 16) / 2, 0);
        lv_obj_set_style_pad_bottom(ta, (fh - 16) / 2, 0);
        lv_obj_set_style_pad_hor(ta, 10, 0);
        lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);

        return ta;
    };

    lv_obj_t* ta_name = make_field(t(K_NOMBRE), t(K_EJ_MI_CUENTA), _fw_name_buf, false);
    _fw_ta_name = ta_name;
    lv_obj_add_event_cb(ta_name, [](lv_event_t* e){
        _show_field_editor(t(K_NOMBRE), _fw_name_buf, VAULT_FIELD_LEN, false, []{
            if (_fw_ta_name && lv_obj_is_valid(_fw_ta_name))
                lv_textarea_set_text(_fw_ta_name, _fw_name_buf);
        });
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* ta_user = make_field(t(K_USUARIO), t(K_USUARIO_PLACEHOLDER), _fw_user_buf, false);
    _fw_ta_user = ta_user;
    lv_obj_add_event_cb(ta_user, [](lv_event_t* e){
        _show_field_editor(t(K_USUARIO), _fw_user_buf, VAULT_FIELD_LEN, false, []{
            if (_fw_ta_user && lv_obj_is_valid(_fw_ta_user))
                lv_textarea_set_text(_fw_ta_user, _fw_user_buf);
        });
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl_p = lv_label_create(body);
    lv_label_set_text(lbl_p, t(K_CONTRASENA));
    lv_obj_set_style_text_color(lbl_p, C_TEXT, 0);
    lv_obj_set_style_text_font(lbl_p, FONT_MD, 0);
    lv_obj_set_size(lbl_p, fw, 20);
    lv_obj_set_style_min_height(lbl_p, 20, 0);
    lv_obj_set_style_max_height(lbl_p, 20, 0);
    lv_obj_set_style_pad_all(lbl_p, 0, 0);

    // Fila: textarea + generar + ojo
    lv_obj_t* pass_row = lv_obj_create(body);
    lv_obj_set_size(pass_row, fw, fh);
    lv_obj_set_style_min_height(pass_row, fh, 0);
    lv_obj_set_style_max_height(pass_row, fh, 0);
    lv_obj_set_style_bg_opa(pass_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pass_row, 0, 0);
    lv_obj_set_style_pad_all(pass_row, 0, 0);
    lv_obj_clear_flag(pass_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pass_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    int eye_w = fh;
    int gen_w = fh;

    lv_obj_t* ta_pass = lv_textarea_create(pass_row);
    _fw_ta_pass = ta_pass;
    lv_obj_set_size(ta_pass, fw - eye_w - gen_w - 8, fh);
    lv_obj_set_pos(ta_pass, 0, 0);
    lv_textarea_set_one_line(ta_pass, true);
    lv_textarea_set_max_length(ta_pass, VAULT_FIELD_LEN - 1);
    lv_textarea_set_password_mode(ta_pass, !_fw_pass_visible);
    if (_fw_pass_buf[0]) lv_textarea_set_text(ta_pass, _fw_pass_buf);
    lv_obj_set_style_bg_color(ta_pass, C_CARD, 0);
    lv_obj_set_style_bg_opa(ta_pass, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ta_pass, C_BORDER, 0);
    lv_obj_set_style_border_width(ta_pass, 1, 0);
    lv_obj_set_style_radius(ta_pass, 10, 0);
    lv_obj_set_style_text_color(ta_pass, C_TEXT, 0);
    lv_obj_set_style_text_font(ta_pass, FONT_SM, 0);
    lv_obj_set_style_pad_top(ta_pass, (fh - 16) / 2, 0);
    lv_obj_set_style_pad_bottom(ta_pass, (fh - 16) / 2, 0);
    lv_obj_set_style_pad_hor(ta_pass, 10, 0);
    lv_obj_set_scrollbar_mode(ta_pass, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(ta_pass, [](lv_event_t* e){
        _show_field_editor(t(K_CONTRASENA), _fw_pass_buf, VAULT_FIELD_LEN, true, []{
            // Actualizar display del campo
            if (_fw_ta_pass && lv_obj_is_valid(_fw_ta_pass))
                lv_textarea_set_text(_fw_ta_pass, _fw_pass_buf);
        });
    }, LV_EVENT_CLICKED, nullptr);

    // Botón generar — entre textarea y ojo
    lv_obj_t* gen_btn = lv_btn_create(pass_row);
    lv_obj_set_size(gen_btn, gen_w, fh);
    lv_obj_set_pos(gen_btn, fw - eye_w - gen_w - 4, 0);
    lv_obj_set_style_bg_color(gen_btn, C_CARD, 0);
    lv_obj_set_style_bg_opa(gen_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(gen_btn, C_BORDER, 0);
    lv_obj_set_style_border_width(gen_btn, 1, 0);
    lv_obj_set_style_radius(gen_btn, 10, 0);
    lv_obj_set_style_shadow_width(gen_btn, 0, 0);
    lv_obj_set_style_pad_all(gen_btn, 0, 0);
    lv_obj_t* gen_ico = lv_label_create(gen_btn);
    lv_label_set_text(gen_ico, ICO_AUTORENEW); lv_obj_set_style_text_font(gen_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(gen_ico, C_MUTED, 0);
    lv_obj_set_style_text_font(gen_ico, FONT_ICON, 0);
    lv_obj_center(gen_ico);
    lv_obj_clear_flag(gen_ico, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gen_btn, [](lv_event_t* e){
        VaultGenConfig cfg = _load_gen_config();
        char buf[33] = {0};
        _gen_password(buf, cfg.len, cfg.upper, cfg.nums, cfg.syms, cfg.ambig);
        strncpy(_fw_pass_buf, buf, VAULT_FIELD_LEN-1);
        // Actualizar textarea directamente sin recargar la pantalla
        if (_fw_ta_pass) {
            lv_textarea_set_text(_fw_ta_pass, buf);
            lv_textarea_set_password_mode(_fw_ta_pass, false);
            _fw_pass_visible = true;
        }
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* eye_btn = lv_btn_create(pass_row);
    lv_obj_set_size(eye_btn, eye_w, fh);
    lv_obj_set_pos(eye_btn, fw - eye_w, 0);
    lv_obj_set_style_bg_color(eye_btn, C_CARD, 0);
    lv_obj_set_style_bg_opa(eye_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(eye_btn, C_BORDER, 0);
    lv_obj_set_style_border_width(eye_btn, 1, 0);
    lv_obj_set_style_radius(eye_btn, 10, 0);
    lv_obj_set_style_shadow_width(eye_btn, 0, 0);
    lv_obj_set_style_pad_all(eye_btn, 0, 0);
    lv_obj_t* eye_ico_fw = lv_label_create(eye_btn);
    lv_label_set_text(eye_ico_fw, ICO_VISIBILITY); lv_obj_set_style_text_font(eye_ico_fw, FONT_ICON, 0);
    lv_obj_set_style_text_color(eye_ico_fw, C_MUTED, 0);
    lv_obj_center(eye_ico_fw);
    lv_obj_clear_flag(eye_ico_fw, LV_OBJ_FLAG_CLICKABLE);
    struct EyeCtx { lv_obj_t* ta; lv_obj_t* ico; };
    static EyeCtx ectx; ectx = {ta_pass, eye_ico_fw};
    lv_obj_add_event_cb(eye_btn, [](lv_event_t* e){
        EyeCtx* ec = (EyeCtx*)lv_event_get_user_data(e);
        _fw_pass_visible = !_fw_pass_visible;
        lv_textarea_set_password_mode(ec->ta, !_fw_pass_visible);
        lv_label_set_text(ec->ico, _fw_pass_visible ? ICO_VISIBILITY_OFF : ICO_VISIBILITY);
    }, LV_EVENT_CLICKED, &ectx);

    // Notas (o URL) — campo de texto libre, opcional
    lv_obj_t* ta_notes = make_field(t(K_NOTAS_O_URL), t(K_OPCIONAL), _fw_notes_buf, false);
    lv_textarea_set_max_length(ta_notes, VAULT_NOTES_LEN - 1); // make_field usa VAULT_FIELD_LEN por defecto, más chico
    _fw_ta_notes = ta_notes;
    lv_obj_add_event_cb(ta_notes, [](lv_event_t* e){
        _show_field_editor(t(K_NOTAS), _fw_notes_buf, VAULT_NOTES_LEN, false, []{
            if (_fw_ta_notes && lv_obj_is_valid(_fw_ta_notes))
                lv_textarea_set_text(_fw_ta_notes, _fw_notes_buf);
        });
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* sp_c = lv_obj_create(body);
    lv_obj_set_size(sp_c, fw, 8);
    lv_obj_set_style_bg_opa(sp_c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sp_c, 0, 0);
    lv_obj_set_style_pad_all(sp_c, 0, 0);

    lv_obj_t* lbl_c = lv_label_create(body);
    lv_label_set_text(lbl_c, t(K_CATEGORIA));
    lv_obj_set_style_text_color(lbl_c, C_TEXT, 0);
    lv_obj_set_style_text_font(lbl_c, FONT_MD, 0);
    lv_obj_set_size(lbl_c, fw, 20);
    lv_obj_set_style_min_height(lbl_c, 20, 0);
    lv_obj_set_style_max_height(lbl_c, 20, 0);
    lv_obj_set_style_pad_all(lbl_c, 0, 0);

    lv_obj_t* cat_box = lv_obj_create(body);
    lv_obj_set_size(cat_box, fw, fh);
    lv_obj_set_style_bg_color(cat_box, C_CARD, 0);
    lv_obj_set_style_bg_opa(cat_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cat_box, C_BORDER, 0);
    lv_obj_set_style_border_width(cat_box, 1, 0);
    lv_obj_set_style_radius(cat_box, 10, 0);
    lv_obj_set_style_pad_all(cat_box, 0, 0);
    lv_obj_clear_flag(cat_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* cat_val = lv_label_create(cat_box);
    lv_label_set_text(cat_val,
        strlen(_fw_cat_buf) > 0 ? _cat_display_name_raw(vault_cat_name(_fw_cat_buf).c_str()) : t(K_SIN_CATEGORIA));
    lv_obj_set_style_text_color(cat_val,
        strlen(_fw_cat_buf) > 0 ? C_TEXT : C_MUTED, 0);
    lv_obj_set_style_text_font(cat_val, FONT_SM, 0);
    lv_obj_align(cat_val, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* cat_arr = lv_label_create(cat_box);
    lv_label_set_text(cat_arr, ICO_SORT); lv_obj_set_style_text_font(cat_arr, FONT_ICON, 0);
    lv_obj_set_style_text_color(cat_arr, C_MUTED, 0);
    lv_obj_set_style_text_font(cat_arr, FONT_ICON, 0);
    lv_obj_align(cat_arr, LV_ALIGN_RIGHT_MID, -10, 0);

    // Selector de categoría — bottom sheet
    _press_fb(cat_box);
    lv_obj_add_event_cb(cat_box, [](lv_event_t* e){
        _show_cat_selector(true, (lv_obj_t*)lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, cat_val);

    // Contexto para botón guardar
    struct SaveCtx { lv_obj_t *n, *u, *p; };
    static SaveCtx sctx;
    sctx = {ta_name, ta_user, ta_pass};

    // GUARDAR — último elemento
    lv_obj_t* sp_s = lv_obj_create(body);
    lv_obj_set_size(sp_s, fw, 8);
    lv_obj_set_style_bg_opa(sp_s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sp_s, 0, 0);
    lv_obj_set_style_pad_all(sp_s, 0, 0);

    lv_obj_t* btn_save = lv_btn_create(body);
    lv_obj_set_size(btn_save, fw, 40);
    lv_obj_set_style_bg_color(btn_save, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn_save, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_save, 0, 0);
    lv_obj_set_style_radius(btn_save, 10, 0);
    lv_obj_set_style_shadow_width(btn_save, 0, 0);
    _press_fb(btn_save);
    lv_obj_t* save_lbl = lv_label_create(btn_save);
    lv_label_set_text(save_lbl, t(K_GUARDAR));
    lv_obj_set_style_text_color(save_lbl, C_ACCENT_TEXT, 0);
    lv_obj_set_style_text_font(save_lbl, FONT_MD, 0);
    lv_obj_center(save_lbl);

    lv_obj_add_event_cb(btn_save, [](lv_event_t* e){
        SaveCtx* c = (SaveCtx*)lv_event_get_user_data(e);
        const char* n = lv_textarea_get_text(c->n);
        const char* u = lv_textarea_get_text(c->u);
        const char* p = lv_textarea_get_text(c->p);
        // Validación de campos obligatorios con aviso específico de cuál falta.
        if (!strlen(n)) { toast_err(t(K_TOAST_FALTA_NOMBRE));   return; }
        if (!strlen(u)) { toast_err(t(K_TOAST_FALTA_USUARIO));  return; }
        if (!strlen(p)) { toast_err(t(K_TOAST_FALTA_PASSWORD)); return; }
        bool was_editing = _fw_editing;
        char totp_backup[VAULT_FIELD_LEN] = {0};
        if (_fw_editing) {
            VaultEntry old = vault_load(_selected_name);
            if (old.valid) strncpy(totp_backup, old.totp_secret, VAULT_FIELD_LEN - 1);
            vault_delete(_selected_name);
        }
        vault_save_web(n, u, p,
            strlen(_fw_cat_buf) > 0 ? _fw_cat_buf : "none",
            _fw_notes_buf);
        if (totp_backup[0]) vault_set_totp_secret(n, totp_backup);
        memset(_fw_name_buf,  0, sizeof(_fw_name_buf));
        memset(_fw_user_buf,  0, sizeof(_fw_user_buf));
        memset(_fw_pass_buf,  0, sizeof(_fw_pass_buf));
        memset(_fw_cat_buf,   0, sizeof(_fw_cat_buf));
        memset(_fw_notes_buf, 0, sizeof(_fw_notes_buf));
        int which = was_editing ? 1 : 0;
        _show_list();
        lv_async_call([](void* arg){
            toast_ok(t((int)(intptr_t)arg ? K_TOAST_ENTRADA_EDITADA : K_TOAST_ENTRADA_GUARDADA));
        }, (void*)(intptr_t)which);
    }, LV_EVENT_CLICKED, &sctx);

    if (instant) _load_screen_instant(scr); else _load_screen(scr);
}

static void _show_form_secret(bool editing, bool instant) {
    _current_screen = SCREEN_FORM_SECRET; _kb_active = false;
    _fs_editing = editing;
    if (editing) {
        VaultEntry ex = vault_load(_selected_name);
        if (ex.valid) {
            strncpy(_fs_name_buf,   ex.name,        VAULT_FIELD_LEN-1);
            strncpy(_fs_secret_buf, ex.secret,      VAULT_FIELD_LEN-1);
            strncpy(_fs_cat_buf,    ex.category_id, VAULT_CAT_LEN-1);
            strncpy(_fs_notes_buf,  ex.notes,       VAULT_NOTES_LEN-1);
        }
        // Entrada guardada como "solo nota" → toggle activo al editar
        _fs_note_only = ex.valid && ex.secret[0] == '\0';
    } else {
        _fs_note_only = false;
    }

    // Snapshot para detectar cambios sin guardar al presionar "atrás"
    static char _fs_base_name[VAULT_FIELD_LEN], _fs_base_secret[VAULT_FIELD_LEN],
                _fs_base_notes[VAULT_NOTES_LEN];
    strncpy(_fs_base_name,   _fs_name_buf,   VAULT_FIELD_LEN-1);
    strncpy(_fs_base_secret, _fs_secret_buf, VAULT_FIELD_LEN-1);
    strncpy(_fs_base_notes,  _fs_notes_buf,  VAULT_NOTES_LEN-1);

    _fs_kb    = nullptr;
    _fs_btn_x = nullptr;
    _fs_ta_secret = nullptr;

    lv_obj_t* scr = _make_screen();

    // ── Header ────────────────────────────────────────────────
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_b = lv_btn_create(hdr);
    lv_obj_set_size(btn_b, 40, 44); lv_obj_set_pos(btn_b, 0, 0);
    lv_obj_set_style_bg_opa(btn_b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_b, 0, 0); lv_obj_set_style_border_width(btn_b, 0, 0);
    lv_obj_t* b_ico = lv_label_create(btn_b);
    lv_label_set_text(b_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(b_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(b_ico, C_TEXT, 0); lv_obj_center(b_ico);
    _press_fb(btn_b);
    lv_obj_add_event_cb(btn_b, [](lv_event_t* e){
        bool dirty = strcmp(_fs_name_buf,   _fs_base_name)   != 0 ||
                     strcmp(_fs_secret_buf, _fs_base_secret) != 0 ||
                     strcmp(_fs_notes_buf,  _fs_base_notes)  != 0;
        auto discard = []{
            memset(_fs_name_buf,   0, sizeof(_fs_name_buf));
            memset(_fs_secret_buf, 0, sizeof(_fs_secret_buf));
            memset(_fs_notes_buf,  0, sizeof(_fs_notes_buf));
            _fs_editing ? _show_detail(_selected_name) : _show_list();
        };
        if (dirty) _show_discard_confirm(discard);
        else       discard();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* hdr_title = lv_label_create(hdr);
    lv_label_set_text(hdr_title, editing ? t(K_EDITAR_OBJETO) : t(K_NUEVO_OBJETO));
    lv_obj_set_style_text_color(hdr_title, C_TEXT, 0);
    lv_obj_set_style_text_font(hdr_title, FONT_MD, 0);
    lv_obj_align(hdr_title, LV_ALIGN_CENTER, 0, 0);

    // Sin btn_x — teclado en editor dedicado
    _divider(scr, 44);

    // ── Body ────────────────────────────────────────────────
    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_W, UI_H - 44); lv_obj_set_pos(body, 0, 44);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 16, 0); lv_obj_set_style_pad_top(body, 6, 0);
    lv_obj_set_style_pad_bottom(body, 8, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_row(body, 5, 0);

    int fw = UI_W - 32;
    int fh = 40;

    auto make_field_s = [&](const char* cap, const char* ph, const char* pre, bool pw) -> lv_obj_t* {
        lv_obj_t* lbl = lv_label_create(body);
        lv_label_set_text(lbl, cap);
        lv_obj_set_style_text_color(lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl, FONT_MD, 0);
        lv_obj_set_size(lbl, fw, 16);
        lv_obj_set_style_min_height(lbl, 16, 0);
        lv_obj_set_style_max_height(lbl, 16, 0);
        lv_obj_set_style_pad_all(lbl, 0, 0);

        lv_obj_t* ta = lv_textarea_create(body);
        lv_obj_set_size(ta, fw, fh);
        lv_obj_set_style_min_height(ta, fh, 0);
        lv_obj_set_style_max_height(ta, fh, 0);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_max_length(ta, VAULT_FIELD_LEN - 1);
        lv_textarea_set_placeholder_text(ta, ph);
        if (pw) lv_textarea_set_password_mode(ta, true);
        if (pre && strlen(pre) > 0) lv_textarea_set_text(ta, pre);
        lv_obj_set_style_bg_color(ta, C_CARD, 0); lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(ta, C_BORDER, 0); lv_obj_set_style_border_width(ta, 1, 0);
        lv_obj_set_style_radius(ta, 10, 0);
        lv_obj_set_style_text_color(ta, C_TEXT, 0); lv_obj_set_style_text_font(ta, FONT_MD, 0);
        lv_obj_set_style_pad_top(ta, (fh - 16) / 2, 0); lv_obj_set_style_pad_bottom(ta, (fh - 16) / 2, 0);
        lv_obj_set_style_pad_hor(ta, 10, 0);
        lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);
        return ta;
    };

    // Nombre
    static lv_obj_t* _fs_ta_name = nullptr;
    lv_obj_t* ta_name = make_field_s(t(K_NOMBRE), t(K_EJ_TARJETA_BBVA), _fs_name_buf, false);
    _fs_ta_name = ta_name;
    lv_obj_add_event_cb(ta_name, [](lv_event_t* e){
        _show_field_editor(t(K_NOMBRE), _fs_name_buf, VAULT_FIELD_LEN, false, []{
            if (_fs_ta_name && lv_obj_is_valid(_fs_ta_name))
                lv_textarea_set_text(_fs_ta_name, _fs_name_buf);
        });
    }, LV_EVENT_CLICKED, nullptr);

    // Toggle "Guardar solo nota": oculta el secreto y hace la nota obligatoria
    struct NoteOnlyUI { lv_obj_t *lbl_s, *pass_row, *ta_notes; };
    static NoteOnlyUI _fs_no_ui;
    _fs_no_ui = {nullptr, nullptr, nullptr}; // se rellena más abajo

    lv_obj_t* no_row = lv_obj_create(body);
    lv_obj_set_size(no_row, fw, 30);
    lv_obj_set_style_min_height(no_row, 30, 0);
    lv_obj_set_style_max_height(no_row, 30, 0);
    lv_obj_set_style_bg_opa(no_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(no_row, 0, 0);
    lv_obj_set_style_pad_all(no_row, 0, 0);
    lv_obj_clear_flag(no_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* no_lbl = lv_label_create(no_row);
    lv_label_set_text(no_lbl, t(K_SOLO_NOTA));
    lv_obj_set_style_text_color(no_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(no_lbl, FONT_SM, 0);
    lv_obj_align(no_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* no_sw = lv_switch_create(no_row);
    lv_obj_set_size(no_sw, 44, 24);
    lv_obj_align(no_sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(no_sw, C_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (_fs_note_only) lv_obj_add_state(no_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(no_sw, [](lv_event_t* e){
        lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
        _fs_note_only = lv_obj_has_state(sw, LV_STATE_CHECKED);
        NoteOnlyUI* u = (NoteOnlyUI*)lv_event_get_user_data(e);
        if (u->lbl_s && lv_obj_is_valid(u->lbl_s) && u->pass_row && lv_obj_is_valid(u->pass_row)) {
            if (_fs_note_only) {
                lv_obj_add_flag(u->lbl_s,    LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(u->pass_row, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(u->lbl_s,    LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(u->pass_row, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (u->ta_notes && lv_obj_is_valid(u->ta_notes))
            lv_textarea_set_placeholder_text(u->ta_notes, _fs_note_only ? "" : t(K_OPCIONAL));
    }, LV_EVENT_VALUE_CHANGED, &_fs_no_ui);

    // Secreto con ojo y generador
    lv_obj_t* lbl_s = lv_label_create(body);
    lv_label_set_text(lbl_s, t(K_SECRETO));
    lv_obj_set_style_text_color(lbl_s, C_TEXT, 0); lv_obj_set_style_text_font(lbl_s, FONT_MD, 0);
    lv_obj_set_size(lbl_s, fw, 16); lv_obj_set_style_min_height(lbl_s, 16, 0);
    lv_obj_set_style_max_height(lbl_s, 16, 0); lv_obj_set_style_pad_all(lbl_s, 0, 0);

    lv_obj_t* pass_row = lv_obj_create(body);
    lv_obj_set_size(pass_row, fw, fh);
    lv_obj_set_style_min_height(pass_row, fh, 0);
    lv_obj_set_style_max_height(pass_row, fh, 0);
    lv_obj_set_style_bg_opa(pass_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pass_row, 0, 0);
    lv_obj_set_style_pad_all(pass_row, 0, 0);
    lv_obj_clear_flag(pass_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pass_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    int eye_w = fh, gen_w_s = fh;

    lv_obj_t* ta_secret = lv_textarea_create(pass_row);
    _fs_ta_secret = ta_secret;
    lv_obj_set_size(ta_secret, fw - eye_w - gen_w_s - 8, fh);
    lv_obj_set_style_min_height(ta_secret, fh, 0);
    lv_obj_set_style_max_height(ta_secret, fh, 0);
    lv_obj_set_pos(ta_secret, 0, 0);
    lv_textarea_set_one_line(ta_secret, true); lv_textarea_set_max_length(ta_secret, VAULT_FIELD_LEN - 1);
    lv_textarea_set_password_mode(ta_secret, !_fs_secret_visible);
    if (_fs_secret_buf[0]) lv_textarea_set_text(ta_secret, _fs_secret_buf);
    lv_obj_set_style_bg_color(ta_secret, C_CARD, 0); lv_obj_set_style_bg_opa(ta_secret, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ta_secret, C_BORDER, 0); lv_obj_set_style_border_width(ta_secret, 1, 0);
    lv_obj_set_style_radius(ta_secret, 10, 0);
    lv_obj_set_style_text_color(ta_secret, C_TEXT, 0); lv_obj_set_style_text_font(ta_secret, FONT_MD, 0);
    lv_obj_set_style_pad_top(ta_secret, (fh-16)/2, 0); lv_obj_set_style_pad_bottom(ta_secret, (fh-16)/2, 0);
    lv_obj_set_style_pad_hor(ta_secret, 10, 0); lv_obj_set_scrollbar_mode(ta_secret, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(ta_secret, [](lv_event_t* e){
        _show_field_editor(t(K_SECRETO), _fs_secret_buf, VAULT_FIELD_LEN, true, []{
            if (_fs_ta_secret && lv_obj_is_valid(_fs_ta_secret))
                lv_textarea_set_text(_fs_ta_secret, _fs_secret_buf);
        });
    }, LV_EVENT_CLICKED, nullptr);

    // Botón generar
    lv_obj_t* gen_btn_s = lv_btn_create(pass_row);
    lv_obj_set_size(gen_btn_s, gen_w_s, fh);
    lv_obj_set_style_min_height(gen_btn_s, fh, 0);
    lv_obj_set_style_max_height(gen_btn_s, fh, 0);
    lv_obj_set_pos(gen_btn_s, fw - eye_w - gen_w_s - 4, 0);
    lv_obj_set_style_bg_color(gen_btn_s, C_CARD, 0); lv_obj_set_style_bg_opa(gen_btn_s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(gen_btn_s, C_BORDER, 0); lv_obj_set_style_border_width(gen_btn_s, 1, 0);
    lv_obj_set_style_radius(gen_btn_s, 10, 0); lv_obj_set_style_shadow_width(gen_btn_s, 0, 0);
    lv_obj_set_style_pad_all(gen_btn_s, 0, 0); _press_fb(gen_btn_s);
    lv_obj_t* gen_ico_s = lv_label_create(gen_btn_s);
    lv_label_set_text(gen_ico_s, ICO_AUTORENEW); lv_obj_set_style_text_font(gen_ico_s, FONT_ICON, 0);
    lv_obj_set_style_text_color(gen_ico_s, C_MUTED, 0); lv_obj_center(gen_ico_s);
    lv_obj_clear_flag(gen_ico_s, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gen_btn_s, [](lv_event_t* e){
        VaultGenConfig cfg = _load_gen_config();
        char buf[33] = {0};
        _gen_password(buf, cfg.len, cfg.upper, cfg.nums, cfg.syms, cfg.ambig);
        strncpy(_fs_secret_buf, buf, VAULT_FIELD_LEN-1);
        if (_fs_ta_secret && lv_obj_is_valid(_fs_ta_secret)) {
            lv_textarea_set_text(_fs_ta_secret, buf);
            lv_textarea_set_password_mode(_fs_ta_secret, false);
            _fs_secret_visible = true;
        }
    }, LV_EVENT_CLICKED, nullptr);

    // Botón ojo
    lv_obj_t* eye_btn = lv_btn_create(pass_row);
    lv_obj_set_size(eye_btn, eye_w, fh);
    lv_obj_set_style_min_height(eye_btn, fh, 0);
    lv_obj_set_style_max_height(eye_btn, fh, 0);
    lv_obj_set_pos(eye_btn, fw - eye_w, 0);
    lv_obj_set_style_bg_color(eye_btn, C_CARD, 0); lv_obj_set_style_bg_opa(eye_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(eye_btn, C_BORDER, 0); lv_obj_set_style_border_width(eye_btn, 1, 0);
    lv_obj_set_style_radius(eye_btn, 10, 0); lv_obj_set_style_shadow_width(eye_btn, 0, 0);
    lv_obj_set_style_pad_all(eye_btn, 0, 0); _press_fb(eye_btn);
    lv_obj_t* eye_ico = lv_label_create(eye_btn);
    lv_label_set_text(eye_ico, ICO_VISIBILITY); lv_obj_set_style_text_font(eye_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(eye_ico, C_MUTED, 0); lv_obj_center(eye_ico);
    lv_obj_clear_flag(eye_ico, LV_OBJ_FLAG_CLICKABLE);
    struct SecEyeCtx { lv_obj_t* ta; lv_obj_t* ico; };
    static SecEyeCtx seyectx; seyectx = {ta_secret, eye_ico};
    lv_obj_add_event_cb(eye_btn, [](lv_event_t* e){
        SecEyeCtx* d = (SecEyeCtx*)lv_event_get_user_data(e);
        _fs_secret_visible = !_fs_secret_visible;
        lv_textarea_set_password_mode(d->ta, !_fs_secret_visible);
        lv_label_set_text(d->ico, _fs_secret_visible ? ICO_VISIBILITY_OFF : ICO_VISIBILITY);
    }, LV_EVENT_CLICKED, &seyectx);

    // Notas (o URL) — campo de texto libre, opcional
    lv_obj_t* ta_notes_s = make_field_s(t(K_NOTAS_O_URL), t(K_OPCIONAL), _fs_notes_buf, false);
    lv_textarea_set_max_length(ta_notes_s, VAULT_NOTES_LEN - 1);
    _fs_ta_notes = ta_notes_s;
    lv_obj_add_event_cb(ta_notes_s, [](lv_event_t* e){
        _show_field_editor(t(K_NOTAS), _fs_notes_buf, VAULT_NOTES_LEN, false, []{
            if (_fs_ta_notes && lv_obj_is_valid(_fs_ta_notes))
                lv_textarea_set_text(_fs_ta_notes, _fs_notes_buf);
        });
    }, LV_EVENT_CLICKED, nullptr);

    // Conectar el toggle "solo nota" con sus objetivos y aplicar estado inicial
    _fs_no_ui = {lbl_s, pass_row, ta_notes_s};
    if (_fs_note_only) {
        lv_obj_add_flag(lbl_s,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pass_row, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_placeholder_text(ta_notes_s, "");
    }

    // Espaciador entre contraseña y categoría
    lv_obj_t* sp_pc = lv_obj_create(body);
    lv_obj_set_size(sp_pc, fw, 4);
    lv_obj_set_style_min_height(sp_pc, 4, 0);
    lv_obj_set_style_max_height(sp_pc, 4, 0);
    lv_obj_set_style_bg_opa(sp_pc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sp_pc, 0, 0);
    lv_obj_set_style_pad_all(sp_pc, 0, 0);

    // Categoría
    lv_obj_t* lbl_cs = lv_label_create(body);
    lv_label_set_text(lbl_cs, t(K_CATEGORIA));
    lv_obj_set_style_text_color(lbl_cs, C_TEXT, 0); lv_obj_set_style_text_font(lbl_cs, FONT_MD, 0);
    lv_obj_set_size(lbl_cs, fw, 16); lv_obj_set_style_min_height(lbl_cs, 16, 0);
    lv_obj_set_style_max_height(lbl_cs, 16, 0); lv_obj_set_style_pad_all(lbl_cs, 0, 0);

    lv_obj_t* cat_box_s = lv_obj_create(body);
    lv_obj_set_size(cat_box_s, fw, fh);
    lv_obj_set_style_bg_color(cat_box_s, C_CARD, 0); lv_obj_set_style_bg_opa(cat_box_s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cat_box_s, C_BORDER, 0); lv_obj_set_style_border_width(cat_box_s, 1, 0);
    lv_obj_set_style_radius(cat_box_s, 10, 0); lv_obj_set_style_pad_all(cat_box_s, 0, 0);
    lv_obj_clear_flag(cat_box_s, LV_OBJ_FLAG_SCROLLABLE); _press_fb(cat_box_s);

    lv_obj_t* cat_val_s = lv_label_create(cat_box_s);
    lv_label_set_text(cat_val_s, strlen(_fs_cat_buf) > 0 ? _cat_display_name_raw(vault_cat_name(_fs_cat_buf).c_str()) : t(K_SIN_CATEGORIA));
    lv_obj_set_style_text_color(cat_val_s, strlen(_fs_cat_buf) > 0 ? C_TEXT : C_MUTED, 0);
    lv_obj_set_style_text_font(cat_val_s, FONT_SM, 0); lv_obj_align(cat_val_s, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* cat_arr_s = lv_label_create(cat_box_s);
    lv_label_set_text(cat_arr_s, ICO_SORT); lv_obj_set_style_text_font(cat_arr_s, FONT_ICON, 0);
    lv_obj_set_style_text_color(cat_arr_s, C_MUTED, 0); lv_obj_align(cat_arr_s, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(cat_box_s, [](lv_event_t* e){
        _show_cat_selector(false, (lv_obj_t*)lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, cat_val_s);

    // Guardar
    lv_obj_t* sp_save = lv_obj_create(body);
    lv_obj_set_size(sp_save, fw, 2); lv_obj_set_style_bg_opa(sp_save, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sp_save, 0, 0); lv_obj_set_style_pad_all(sp_save, 0, 0);

    lv_obj_t* btn_save = lv_btn_create(body);
    lv_obj_set_size(btn_save, fw, 40);
    lv_obj_set_style_bg_color(btn_save, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn_save, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_save, 0, 0); lv_obj_set_style_radius(btn_save, 10, 0);
    lv_obj_set_style_shadow_width(btn_save, 0, 0); _press_fb(btn_save);
    lv_obj_t* save_lbl = lv_label_create(btn_save);
    lv_label_set_text(save_lbl, t(K_GUARDAR));
    lv_obj_set_style_text_color(save_lbl, C_ACCENT_TEXT, 0);
    lv_obj_set_style_text_font(save_lbl, FONT_MD, 0); lv_obj_center(save_lbl);

    struct SaveCtxS { lv_obj_t *n, *s; };
    static SaveCtxS sctx; sctx = {ta_name, ta_secret};
    lv_obj_add_event_cb(btn_save, [](lv_event_t* e){
        SaveCtxS* c = (SaveCtxS*)lv_event_get_user_data(e);
        const char* n = lv_textarea_get_text(c->n);
        const char* s = lv_textarea_get_text(c->s);
        if (!strlen(n)) { toast_err(t(K_TOAST_FALTA_NOMBRE)); return; }
        if (_fs_note_only) {
            if (!strlen(_fs_notes_buf)) { toast_err(t(K_TOAST_FALTA_NOTA)); return; }
        } else {
            if (!strlen(s)) { toast_err(t(K_TOAST_FALTA_PASSWORD)); return; }
        }
        bool was_editing = _fs_editing;
        char totp_backup[VAULT_FIELD_LEN] = {0};
        if (_fs_editing) {
            VaultEntry old = vault_load(_selected_name);
            if (old.valid) strncpy(totp_backup, old.totp_secret, VAULT_FIELD_LEN - 1);
            vault_delete(_selected_name);
        }
        vault_save_secret(n, _fs_note_only ? "" : s,
                          strlen(_fs_cat_buf) > 0 ? _fs_cat_buf : "none", _fs_notes_buf);
        if (totp_backup[0]) vault_set_totp_secret(n, totp_backup);
        memset(_fs_name_buf,   0, sizeof(_fs_name_buf));
        memset(_fs_secret_buf, 0, sizeof(_fs_secret_buf));
        memset(_fs_cat_buf,    0, sizeof(_fs_cat_buf));
        memset(_fs_notes_buf,  0, sizeof(_fs_notes_buf));
        int which = was_editing ? 1 : 0;
        _show_list();
        lv_async_call([](void* arg){
            toast_ok(t((int)(intptr_t)arg ? K_TOAST_ENTRADA_EDITADA : K_TOAST_ENTRADA_GUARDADA));
        }, (void*)(intptr_t)which);
    }, LV_EVENT_CLICKED, &sctx);

    if (instant) _load_screen_instant(scr); else _load_screen(scr);
}

// ── PANTALLA: Teclado fullscreen ──────────────────────────────
static void _show_keyboard_fullscreen(const char* field_label,
    const char* current_val, bool is_password,
    int return_form, int field_idx) {

    _kb_return_form = return_form;
    _kb_field_idx   = field_idx;
    _kb_is_password = is_password;

    lv_obj_t* scr = _make_screen();

    lv_obj_t* btn_b = lv_btn_create(scr);
    lv_obj_set_size(btn_b, 36, 36);
    lv_obj_set_pos(btn_b, 8, 8);
    lv_obj_set_style_bg_opa(btn_b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_b, 0, 0);
    lv_obj_set_style_border_width(btn_b, 0, 0);
    lv_obj_t* b_ico = lv_label_create(btn_b);
    lv_label_set_text(b_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(b_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(b_ico, C_TEXT, 0);
    lv_obj_set_style_text_font(b_ico, FONT_ICON, 0);
    lv_obj_center(b_ico);
    _press_fb(btn_b);
    lv_obj_add_event_cb(btn_b, [](lv_event_t* e){
        if (_kb_return_form == 0) _show_form_web(_fw_editing);
        else                      _show_form_secret(_fs_editing);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* hdr_lbl = lv_label_create(scr);
    lv_label_set_text(hdr_lbl, field_label);
    lv_obj_set_style_text_color(hdr_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(hdr_lbl, FONT_MD, 0);
    lv_obj_set_pos(hdr_lbl, 44, 14);

    _divider(scr, 40);

    // Textarea
    lv_obj_t* ta = lv_textarea_create(scr);
    lv_obj_set_size(ta, is_password ? UI_W - 60 : UI_W - 32, 36);
    lv_obj_set_pos(ta, 16, 48);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, VAULT_FIELD_LEN - 1);
    if (is_password) lv_textarea_set_password_mode(ta, true);
    if (current_val && strlen(current_val) > 0)
        lv_textarea_set_text(ta, current_val);
    lv_obj_set_style_bg_color(ta, C_BG, 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ta, 0, 0);
    lv_obj_set_style_border_side(ta, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(ta, C_BORDER, 0);
    lv_obj_set_style_border_width(ta, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(ta, C_TEXT, 0);
    lv_obj_set_style_text_font(ta, FONT_LG, 0);
    lv_obj_set_style_radius(ta, 0, 0);
    lv_obj_set_style_pad_all(ta, 4, 0);

    if (is_password) {
        lv_obj_t* eye = lv_btn_create(scr);
        lv_obj_set_size(eye, 36, 36);
        lv_obj_set_pos(eye, UI_W - 12 - 36, 48);
        lv_obj_set_style_bg_opa(eye, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(eye, 0, 0);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_t* eye_ico = lv_label_create(eye);
        lv_label_set_text(eye_ico, ICO_VISIBILITY); lv_obj_set_style_text_font(eye_ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(eye_ico, C_MUTED, 0);
        lv_obj_center(eye_ico);
        lv_obj_add_event_cb(eye, [](lv_event_t* e){
            lv_obj_t* t = (lv_obj_t*)lv_event_get_user_data(e);
            lv_textarea_set_password_mode(t, !lv_textarea_get_password_mode(t));
        }, LV_EVENT_CLICKED, ta);
    }

    // Teclado en tamaño natural al fondo
    lv_obj_t* kb = lv_keyboard_create(scr);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb, C_BG, 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_bg_color(kb, C_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, C_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 6, LV_PART_ITEMS);
    lv_keyboard_set_textarea(kb, ta);

    lv_obj_add_event_cb(kb, [](lv_event_t* e){
        lv_obj_t* kb_obj = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* ta_ptr = lv_keyboard_get_textarea(kb_obj);
        if (!ta_ptr) return;
        const char* val = lv_textarea_get_text(ta_ptr);
        if (_kb_return_form == 0) {
            if      (_kb_field_idx == 0) strncpy(_fw_name_buf, val, VAULT_FIELD_LEN-1);
            else if (_kb_field_idx == 1) strncpy(_fw_user_buf, val, VAULT_FIELD_LEN-1);
            else if (_kb_field_idx == 2) strncpy(_fw_pass_buf, val, VAULT_FIELD_LEN-1);
            _show_form_web(_fw_editing);
        } else {
            if      (_kb_field_idx == 0) strncpy(_fs_name_buf,   val, VAULT_FIELD_LEN-1);
            else if (_kb_field_idx == 1) strncpy(_fs_secret_buf, val, VAULT_FIELD_LEN-1);
            _show_form_secret(_fs_editing);
        }
    }, LV_EVENT_READY, nullptr);

    _load_screen(scr);
}

