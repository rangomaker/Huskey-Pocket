#pragma once
// =============================================================
//  kb_manager.h — Manager unificado de teclado
//  Despacha a custom_kb o custom_kb_t9 según vault_get_t9_mode()
//  Incluir DESPUÉS de custom_kb.h y custom_kb_t9.h
// =============================================================
#include "vault.h"
#include "colors.h"

static void _kb_apply_cursor(lv_obj_t* ta) {
    if (!ta || !lv_obj_is_valid(ta)) return;
    lv_obj_set_style_border_color(ta, C_ACCENT, LV_PART_CURSOR);
    lv_obj_set_style_border_width(ta, 1, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(ta, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_border_side(ta, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
}

static lv_obj_t* kb_create(lv_obj_t* parent, lv_obj_t* ta, void(*ok_cb)(void)) {
    _kb_apply_cursor(ta);
    if (vault_get_t9_mode()) return custom_kb_t9_create(parent, ta, ok_cb);
    return custom_kb_create(parent, ta, ok_cb);
}
static void kb_reset()          { custom_kb_reset(); custom_kb_t9_reset(); }
static void kb_show()           { if (vault_get_t9_mode()) custom_kb_t9_show(); else custom_kb_show(); }
static void kb_hide()           { if (vault_get_t9_mode()) custom_kb_t9_hide(); else custom_kb_hide(); }
static void kb_set_ta(lv_obj_t* ta) {
    _kb_apply_cursor(ta);
    if (vault_get_t9_mode()) custom_kb_t9_set_ta(ta); else custom_kb_set_ta(ta);
}
static lv_obj_t* kb_get_ta()   { return vault_get_t9_mode() ? custom_kb_t9_get_ta() : custom_kb_get_ta(); }
static int  kb_height()         { return vault_get_t9_mode() ? T9_H : CKB_H; }
