#pragma once
// =============================================================
//  ui_qr.h — pantalla de compartir por QR (usuario/contraseña/
//  secreto + notas en texto plano). Extraído de ui_main.h.
//  Depende de helpers/colores/fuentes definidos antes en ui_main.h
//  y de _selected_name (global de ui_list).
// =============================================================
#include <lvgl.h>
#include "i18n.h"

static void _show_qr_share(const char* title, const char* text) {
    lv_obj_t* scr = _make_screen();

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
    lv_obj_add_event_cb(btn_b, [](lv_event_t* e){ _show_detail(_selected_name); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* hdr_title = lv_label_create(hdr);
    lv_label_set_text(hdr_title, title);
    lv_obj_set_style_text_color(hdr_title, C_TEXT, 0);
    lv_obj_set_style_text_font(hdr_title, FONT_MD, 0);
    lv_obj_align(hdr_title, LV_ALIGN_CENTER, 0, 0);

    _divider(scr, 44);

    // Tarjeta blanca de fondo — un QR necesita alto contraste real
    // (negro sobre blanco) para escanear bien, sin importar el tema.
    int qr_size = 180;
    lv_obj_t* qr_card = lv_obj_create(scr);
    lv_obj_set_size(qr_card, qr_size + 24, qr_size + 24);
    lv_obj_align(qr_card, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_style_bg_color(qr_card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(qr_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(qr_card, 0, 0);
    lv_obj_set_style_radius(qr_card, 12, 0);
    lv_obj_clear_flag(qr_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* qr = lv_qrcode_create(qr_card, qr_size, lv_color_hex(0x000000), lv_color_hex(0xFFFFFF));
    lv_obj_center(qr);
    lv_qrcode_update(qr, text, strlen(text));

    lv_obj_t* warn = lv_label_create(scr);
    lv_label_set_text(warn, t(K_QR_WARNING));
    lv_obj_set_style_text_color(warn, C_MUTED, 0);
    lv_obj_set_style_text_font(warn, FONT_SM, 0);
    lv_obj_set_style_text_align(warn, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(warn, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(warn, UI_W - 48);
    lv_obj_align(warn, LV_ALIGN_TOP_MID, 0, 64 + qr_size + 24 + 12);

    _load_screen_instant(scr);
}
