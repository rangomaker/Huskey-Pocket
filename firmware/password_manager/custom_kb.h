#pragma once
// =============================================================
//  custom_kb.h — Teclado dividido KeyVault
//  240x320, LVGL 8.x — 6 slides
// =============================================================
#include <lvgl.h>
#include "colors.h"
#include "fonts.h"
#include "vibro_manager.h"

#define CKB_W      240
#define CKB_H      192
#define CKB_ROW_H   40
#define CKB_GAP      3
#define CKB_PAD      0

// ── Estado ────────────────────────────────────────────────────
static lv_obj_t*  _ckb_root   = nullptr;
static lv_obj_t*  _ckb_cont   = nullptr; // contenedor deslizante (ancho = 6*CKB_W)
static lv_obj_t*  _ckb_ind    = nullptr; // label indicador de modo
static lv_obj_t*  _ckb_ta     = nullptr; // textarea activo
static int        _ckb_slide  = 0;
static int        _ckb_shift  = 0;      // 0=off 1=once 2=locked
static void     (*_ckb_ok_cb)(void) = nullptr;

// Arrays de slides construidos
static lv_obj_t*  _ckb_slides[6] = {nullptr};

// ── Acción de cada tecla ──────────────────────────────────────
// 0=char, 1=backspace, 2=shift, 3=ok, 4=space
// 5=nav_right, 6=nav_left
// 7=mode_123, 8=mode_abc, 9=mode_sym2, 10=mode_sym1
// 11=comma, 12=dot

struct CKBKey { const char* lbl; float w; int act; uint32_t bg; uint32_t fg; };

// Forward declarations
static void _ckb_go(int slide, bool anim);
static void _ckb_rebuild_letters();

struct _CkbFlash { lv_obj_t* b; lv_color_t c; };
static void _ckb_flash(lv_obj_t* btn) {
    _CkbFlash* ctx = new _CkbFlash{btn, lv_obj_get_style_bg_color(btn, 0)};
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E4A6F), 0);
    lv_timer_t* ft = lv_timer_create([](lv_timer_t* t){
        _CkbFlash* c = (_CkbFlash*)t->user_data;
        if (c->b && lv_obj_is_valid(c->b)) lv_obj_set_style_bg_color(c->b, c->c, 0);
        delete c;
        lv_timer_del(t);
    }, 150, ctx);
    lv_timer_set_repeat_count(ft, 1);
}

// ── Agregar texto ─────────────────────────────────────────────
static void _ckb_add(const char* c) {
    if (!_ckb_ta) return;
    lv_textarea_add_text(_ckb_ta, c);
    if (_ckb_shift == 1) {
        _ckb_shift = 0;
        _ckb_rebuild_letters();
    }
}

// ── Crear una tecla ───────────────────────────────────────────

// ── Crear fila ────────────────────────────────────────────────
static lv_obj_t* _ckb_make_row(lv_obj_t* parent, const CKBKey* keys, int n) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, CKB_W - CKB_PAD*2, CKB_ROW_H);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Calcular ancho total de unidades
    float total_w = 0;
    for (int i = 0; i < n; i++) total_w += keys[i].w;
    if (n <= 0 || total_w <= 0) return row; // fila vacía o sin ancho: nada que dibujar
    int avail = (CKB_W - CKB_PAD*2) - (n-1)*CKB_GAP;
    int base_w = avail / (int)total_w;

    int x = 0;
    for (int i = 0; i < n; i++) {
        int w = (int)(keys[i].w * base_w);
        if (i == n-1) w = (CKB_W - CKB_PAD*2) - x;
        lv_obj_t* btn = lv_obj_create(row);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(btn, x, 0);
        lv_obj_set_size(btn, w, CKB_ROW_H);
        lv_obj_set_style_bg_color(btn,
            keys[i].bg ? lv_color_hex(keys[i].bg) :
            keys[i].act == 3 ? C_ACCENT :
            keys[i].act == 2 || keys[i].act >= 4 ? lv_color_hex(0x3A3A3C) :
            lv_color_hex(0x2C2C2E), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        // Borde azul en shift modo once (estado 1)
        if (keys[i].act == 2 && _ckb_shift == 1) {
            lv_obj_set_style_border_color(btn, C_ACCENT, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
        } else {
            lv_obj_set_style_border_width(btn, 0, 0);
        }
        lv_obj_set_style_radius(btn, 7, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_t* lbl = lv_label_create(btn);
        // Para acciones de ícono, usar el ICO_ correcto independiente del lbl
        const char* display_lbl =
            keys[i].act == 1 ? ICO_BACKSPACE :
            keys[i].act == 2 ? ICO_CAPSLOCK :
            keys[i].act == 5 ? ICO_CHEVRON_RIGHT :
            keys[i].act == 6 ? ICO_CHEVRON_LEFT :
            keys[i].lbl;
        lv_label_set_text(lbl, display_lbl);
        lv_obj_set_style_text_color(lbl,
            keys[i].fg ? lv_color_hex(keys[i].fg) :
            keys[i].act == 3 ? lv_color_hex(0xFFFFFF) :
            keys[i].act == 2 && _ckb_shift == 1 ? C_ACCENT :
            keys[i].act == 5 || keys[i].act == 6 ? C_ACCENT :
            keys[i].act == 4 || keys[i].act == 11 || keys[i].act == 12 ? lv_color_hex(0x8E8E93) :
            lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lbl,
            keys[i].act == 1 ? FONT_ICON :
            keys[i].act == 2 ? FONT_ICON :
            keys[i].act == 5 ? FONT_ICON :
            keys[i].act == 6 ? FONT_ICON :
            keys[i].act == 0 ? FONT_MD :
            keys[i].act == 3 ? FONT_MD :
            FONT_SM, 0);
        lv_obj_center(lbl);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(btn, (void*)(intptr_t)keys[i].act);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            lv_obj_t* b  = (lv_obj_t*)lv_event_get_target(e);
            int act      = (int)(intptr_t)lv_obj_get_user_data(b);
            lv_event_code_t code = lv_event_get_code(e);
            if (act == 1 ? code != LV_EVENT_PRESSED : code != LV_EVENT_CLICKED) return;
            vibro_pulse();
            if (act != 2 && act != 3 && (act < 5 || act > 10)) _ckb_flash(b);
            lv_obj_t* lb = lv_obj_get_child(b, 0);
            const char* t = lv_label_get_text(lb);
            switch (act) {
                case 0:  _ckb_add(t); break;
                case 1:  if (_ckb_ta) lv_textarea_del_char(_ckb_ta); break;
                case 2:
                    if (_ckb_shift == 0)      _ckb_shift = 1;
                    else if (_ckb_shift == 1) _ckb_shift = 2;
                    else                      _ckb_shift = 0;
                    _ckb_rebuild_letters();
                    break;
                case 3:  if (_ckb_ok_cb) _ckb_ok_cb(); break;
                case 4:  _ckb_add(" "); break;
                case 5:  _ckb_go(_ckb_slide + 1, true);  break;
                case 6:  _ckb_go(_ckb_slide - 1, true);  break;
                case 7:  _ckb_go(2, false); break;
                case 8:  _ckb_go(0, false); break;
                case 9:  _ckb_go(4, false); break;
                case 10: _ckb_go(2, false); break;
                case 11: _ckb_add(","); break;
                case 12: _ckb_add("."); break;
            }
        }, LV_EVENT_ALL, nullptr);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            int act = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
            if (act == 1 && _ckb_ta) { vibro_pulse(); lv_textarea_del_char(_ckb_ta); }
        }, LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
        x += w + CKB_GAP;
    }
    return row;
}

// ── Construir slide ───────────────────────────────────────────
static void _ckb_build(lv_obj_t* sl, int idx) {
    bool up = _ckb_shift > 0;
    const char* sh_lbl = ICO_CAPSLOCK;
    uint32_t sh_bg = _ckb_shift == 2 ? (lv_color_to32(C_ACCENT)&0xFFFFFF) : 0x3A3A3C; // locked=azul, once=gris

    if (idx == 0) { // Letras izquierda
        static char L0[5][3], L1[5][3], L2[4][3];
        const char* l0[] = {"q","w","e","r","t"};
        const char* l1[] = {"a","s","d","f","g"};
        const char* l2[] = {"z","x","c","v"};
        for (int i=0;i<5;i++) { L0[i][0]=up?toupper(l0[i][0]):l0[i][0]; L0[i][1]=0; }
        for (int i=0;i<5;i++) { L1[i][0]=up?toupper(l1[i][0]):l1[i][0]; L1[i][1]=0; }
        for (int i=0;i<4;i++) { L2[i][0]=up?toupper(l2[i][0]):l2[i][0]; L2[i][1]=0; }
        const CKBKey r0[]={{L0[0],1,0,0,0},{L0[1],1,0,0,0},{L0[2],1,0,0,0},{L0[3],1,0,0,0},{L0[4],1,0,0,0}};
        const CKBKey r1[]={{L1[0],1,0,0,0},{L1[1],1,0,0,0},{L1[2],1,0,0,0},{L1[3],1,0,0,0},{L1[4],1,0,0,0}};
        const CKBKey r2[]={{sh_lbl,1.0f,2,sh_bg,0},{L2[0],1,0,0,0},{L2[1],1,0,0,0},{L2[2],1,0,0,0},{L2[3],1,0,0,0}};
        const CKBKey r3[]=  {{"123",1.0f,7,0x3A3A3C,0},{",",0.8f,11,0,0},{"espacio",2.5f,4,0,0},{ICO_CHEVRON_RIGHT,1.3f,5,0x3A3A3C,lv_color_to32(C_ACCENT)&0xFFFFFF}};
        _ckb_make_row(sl, r0, 5);
        _ckb_make_row(sl, r1, 5);
        _ckb_make_row(sl, r2, 5);
        _ckb_make_row(sl, r3, 4);
    }
    else if (idx == 1) { // Letras derecha
        static char L0[5][4], L1[5][4], L2[3][3];
        const char* l0[] = {"y","u","i","o","p"};
        const char* l1[] = {"h","j","k","l","ñ"};
        const char* l2[] = {"b","n","m"};
        for (int i=0;i<5;i++) { L0[i][0]=up?toupper(l0[i][0]):l0[i][0]; L0[i][1]=0; }
        for (int i=0;i<4;i++) { L1[i][0]=up?toupper(l1[i][0]):l1[i][0]; L1[i][1]=0; }
        // ñ/Ñ
        if (up) { strncpy(L1[4], "Ñ", 4); }
        else    { strncpy(L1[4], "ñ", 4); }
        for (int i=0;i<3;i++) { L2[i][0]=up?toupper(l2[i][0]):l2[i][0]; L2[i][1]=0; }
        const CKBKey r0[]={{L0[0],1,0,0,0},{L0[1],1,0,0,0},{L0[2],1,0,0,0},{L0[3],1,0,0,0},{L0[4],1,0,0,0}};
        const CKBKey r1[]={{L1[0],1,0,0,0},{L1[1],1,0,0,0},{L1[2],1,0,0,0},{L1[3],1,0,0,0},{L1[4],1,0,0,0}};
        const CKBKey r2[]={{L2[0],1,0,0,0},{L2[1],1,0,0,0},{L2[2],1,0,0,0},{".com",1,0,0x3A3A3C,0},{ICO_BACKSPACE,1.0f,1,0x3A3A3C,0}};
        const CKBKey r3[]=  {{ICO_CHEVRON_LEFT,1.3f,6,0x3A3A3C,lv_color_to32(C_ACCENT)&0xFFFFFF},{"espacio",2.5f,4,0,0},{".",0.8f,12,0,0},{"OK",1.5f,3,0,0}};
        _ckb_make_row(sl, r0, 5);
        _ckb_make_row(sl, r1, 5);
        _ckb_make_row(sl, r2, 5);
        _ckb_make_row(sl, r3, 4);
    }
    else if (idx == 2) { // Sym1 izquierda
        const CKBKey r0[]={{"1",1,0,0,0},{"2",1,0,0,0},{"3",1,0,0,0},{"4",1,0,0,0},{"5",1,0,0,0}};
        const CKBKey r1[]={{"@",1,0,0,0},{"#",1,0,0,0},{"$",1,0,0,0},{"_",1,0,0,0},{"&",1,0,0,0}};
        const CKBKey r2[]={{"=\\<",1.0f,9,0x3A3A3C,0},{"*",1,0,0x3A3A3C,0},{"\"",1,0,0x3A3A3C,0},{"'",1,0,0x3A3A3C,0},{"•",1,0,0x3A3A3C,0}};
        const CKBKey r3[]={{"ABC",1.0f,8,0x3A3A3C,0},{",",0.8f,11,0,0},{"espacio",2.5f,4,0,0},{ICO_CHEVRON_RIGHT,1.3f,5,0x3A3A3C,lv_color_to32(C_ACCENT)&0xFFFFFF}};
        _ckb_make_row(sl, r0, 5);
        _ckb_make_row(sl, r1, 5);
        _ckb_make_row(sl, r2, 5);
        _ckb_make_row(sl, r3, 4);
    }
    else if (idx == 3) { // Sym1 derecha
        const CKBKey r0[]={{"6",1,0,0,0},{"7",1,0,0,0},{"8",1,0,0,0},{"9",1,0,0,0},{"0",1,0,0,0}};
        const CKBKey r1[]={{"-",1,0,0,0},{"+",1,0,0,0},{"(",1,0,0,0},{")",1,0,0,0},{"/",1,0,0,0}};
        const CKBKey r2[]={{";",1,0,0x3A3A3C,0},{":",1,0,0x3A3A3C,0},{"!",1,0,0x3A3A3C,0},{"?",1,0,0x3A3A3C,0},{ICO_BACKSPACE,1.0f,1,0x3A3A3C,0}};
        const CKBKey r3[]={{ICO_CHEVRON_LEFT,1.3f,6,0x3A3A3C,lv_color_to32(C_ACCENT)&0xFFFFFF},{"espacio",2.5f,4,0,0},{".",0.8f,12,0,0},{"OK",1.5f,3,0,0}};
        _ckb_make_row(sl, r0, 5);
        _ckb_make_row(sl, r1, 5);
        _ckb_make_row(sl, r2, 5);
        _ckb_make_row(sl, r3, 4);
    }
    else if (idx == 4) { // Sym2 izquierda
            const CKBKey r0[]={{"~",1,0,0,0},{"`",1,0,0,0},{"|",1,0,0,0},{"√",1,0,0,0},{"π",1,0,0,0}};
            const CKBKey r1[]={{"£",1,0,0,0},{"¢",1,0,0,0},{"€",1,0,0,0},{"¥",1,0,0,0},{"^",1,0,0,0}};
            const CKBKey r2[]={{"?123",1.0f,10,0x3A3A3C,0},{"%",1,0,0x3A3A3C,0},{"©",1,0,0x3A3A3C,0},{"®",1,0,0x3A3A3C,0},{"™",1,0,0x3A3A3C,0}};
            const CKBKey r3[]={{"ABC",1.0f,8,0x3A3A3C,0},{",",0.8f,11,0,0},{"espacio",2.5f,4,0,0},{ICO_CHEVRON_RIGHT,1.3f,5,0x3A3A3C,lv_color_to32(C_ACCENT)&0xFFFFFF}};
        _ckb_make_row(sl, r0, 5);
        _ckb_make_row(sl, r1, 5);
        _ckb_make_row(sl, r2, 5);
        _ckb_make_row(sl, r3, 4);
        }
    else if (idx == 5) { // Sym2 derecha
        const CKBKey r0[]={{"÷",1,0,0,0},{"×",1,0,0,0},{"§",1,0,0,0},{"△",1,0,0,0},{"°",1,0,0,0}};
        const CKBKey r1[]={{"=",1,0,0,0},{"{",1,0,0,0},{"}",1,0,0,0},{"[",1,0,0,0},{"]",1,0,0,0}};
        const CKBKey r2[]={{"✓",1,0,0x3A3A3C,0},{"\\",1,0,0x3A3A3C,0},{"`",1,0,0x3A3A3C,0},{"*",1,0,0x3A3A3C,0},{ICO_BACKSPACE,1.0f,1,0x3A3A3C,0}};
        const CKBKey r3[]={{ICO_CHEVRON_LEFT,1.3f,6,0x3A3A3C,lv_color_to32(C_ACCENT)&0xFFFFFF},{"espacio",2.5f,4,0,0},{".",0.8f,12,0,0},{"OK",1.5f,3,0,0}};
        _ckb_make_row(sl, r0, 5);
        _ckb_make_row(sl, r1, 5);
        _ckb_make_row(sl, r2, 5);
        _ckb_make_row(sl, r3, 4);
    }
}

// ── Reconstruir slides de letras (shift cambió) ───────────────
static void _ckb_rebuild_letters() {
    bool up = _ckb_shift > 0;
    uint32_t sh_bg_val = _ckb_shift == 2 ? (lv_color_to32(C_ACCENT)&0xFFFFFF) : 0x3A3A3C;
    lv_color_t sh_border = C_ACCENT;

    for (int s = 0; s < 2; s++) {
        if (!_ckb_slides[s]) continue;
        uint32_t row_cnt = lv_obj_get_child_cnt(_ckb_slides[s]);
        for (uint32_t r = 0; r < row_cnt; r++) {
            lv_obj_t* row = lv_obj_get_child(_ckb_slides[s], r);
            if (!row) continue;
            uint32_t btn_cnt = lv_obj_get_child_cnt(row);
            for (uint32_t b = 0; b < btn_cnt; b++) {
                lv_obj_t* btn = lv_obj_get_child(row, b);
                if (!btn) continue;
                int act = (int)(intptr_t)lv_obj_get_user_data(btn);
                lv_obj_t* lbl = lv_obj_get_child(btn, 0);
                if (!lbl) continue;
                if (act == 0) {
                    const char* txt = lv_label_get_text(lbl);
                    if (txt && txt[0] && !txt[1]) {
                        char c = up ? toupper((unsigned char)txt[0]) : tolower((unsigned char)txt[0]);
                        char buf[2] = {c, '\0'};
                        lv_label_set_text(lbl, buf);
                    }
                } else if (act == 2) {
                    lv_obj_set_style_bg_color(btn, lv_color_hex(sh_bg_val), 0);
                    if (_ckb_shift == 1) {
                        lv_obj_set_style_border_color(btn, sh_border, 0);
                        lv_obj_set_style_border_width(btn, 2, 0);
                        lv_obj_set_style_text_color(lbl, sh_border, 0);
                    } else {
                        lv_obj_set_style_border_width(btn, 0, 0);
                        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
                    }
                }
            }
        }
    }
}

// ── Navegar entre slides ──────────────────────────────────────
static void _ckb_go(int slide, bool anim) {
    if (slide < 0 || slide > 5 || !_ckb_cont) return;

    // Una animación anterior no debe devolver el contenedor al modo previo.
    lv_anim_del(_ckb_cont, (lv_anim_exec_xcb_t)lv_obj_set_x);

    // Determinar el par de slides del modo actual y nuevo
    int cur_pair = (_ckb_slide / 2) * 2; // 0, 2, o 4
    int new_pair = (slide / 2) * 2;

    // Si cambiamos de modo, limpiar el par anterior y construir el nuevo
    if (cur_pair != new_pair) {
        for (int i = cur_pair; i < cur_pair + 2; i++) {
            if (_ckb_slides[i] && i >= 2) // no limpiar letras si venimos de letras
                lv_obj_clean(_ckb_slides[i]);
        }
        // Construir par nuevo si no son letras (letras siempre están construidas)
        if (new_pair >= 2) {
            for (int i = new_pair; i < new_pair + 2; i++) {
                lv_obj_clean(_ckb_slides[i]);
                _ckb_build(_ckb_slides[i], i);
            }
        }
    }

    int target_x = -slide * CKB_W;
    if (anim) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, _ckb_cont);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_set_values(&a, lv_obj_get_x(_ckb_cont), target_x);
        lv_anim_set_time(&a, 200);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    } else {
        lv_obj_set_x(_ckb_cont, target_x);
    }

    _ckb_slide = slide;

    static const char* labels[] = {
        "< LETRAS IZQ","LETRAS DER >",
        "< SIM.1 IZQ","SIM.1 DER >",
        "< SIM.2 IZQ","SIM.2 DER >"
    };
    if (_ckb_ind) lv_label_set_text(_ckb_ind, labels[slide]);
}

// ── Crear el teclado ──────────────────────────────────────────
static lv_obj_t* custom_kb_create(lv_obj_t* parent, lv_obj_t* ta, void(*ok_cb)(void)) {
    // Limpiar estado anterior — punteros de pantalla anterior ya no son válidos
    _ckb_root  = nullptr;
    _ckb_cont  = nullptr;
    _ckb_ind   = nullptr;
    for (int i = 0; i < 6; i++) _ckb_slides[i] = nullptr;

    _ckb_ta    = ta;
    _ckb_ok_cb = ok_cb;
    _ckb_shift = 0;
    _ckb_slide = 0;

    if (ta && lv_obj_is_valid(ta)) {
        lv_obj_set_style_border_color(ta, C_ACCENT, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(ta, 1, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(ta, LV_OPA_TRANSP, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_border_side(ta, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_add_state(ta, LV_STATE_FOCUSED);
    }

    _ckb_root = lv_obj_create(parent);
    lv_obj_set_size(_ckb_root, CKB_W, CKB_H);
    lv_obj_set_style_bg_color(_ckb_root, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_bg_opa(_ckb_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_ckb_root, 0, 0);
    lv_obj_set_style_pad_all(_ckb_root, 0, 0);
    lv_obj_clear_flag(_ckb_root, LV_OBJ_FLAG_SCROLLABLE);

    _ckb_ind = lv_label_create(_ckb_root);
    lv_label_set_text(_ckb_ind, "< LETRAS IZQ");
    lv_obj_set_style_text_color(_ckb_ind, lv_color_hex(0x8E8E93), 0);
    lv_obj_set_style_text_font(_ckb_ind, FONT_SM, 0);
    lv_obj_align(_ckb_ind, LV_ALIGN_TOP_MID, 0, 2);

    _ckb_cont = lv_obj_create(_ckb_root);
    lv_obj_set_size(_ckb_cont, CKB_W * 6, CKB_H - 16);
    lv_obj_set_pos(_ckb_cont, 0, 16);
    lv_obj_set_style_bg_opa(_ckb_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_ckb_cont, 0, 0);
    lv_obj_set_style_pad_all(_ckb_cont, 0, 0);
    lv_obj_clear_flag(_ckb_cont, LV_OBJ_FLAG_SCROLLABLE);

    // Crear solo los primeros 2 slides — lazy loading para el resto
    for (int i = 0; i < 6; i++) {
            lv_obj_t* sl = lv_obj_create(_ckb_cont);
        lv_obj_set_size(sl, CKB_W - CKB_PAD*2, CKB_H - 16);
        lv_obj_set_pos(sl, i * CKB_W + CKB_PAD, 0);
        lv_obj_set_style_bg_opa(sl, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(sl, 0, 0);
        lv_obj_set_style_pad_all(sl, 0, 0);
        lv_obj_set_style_pad_row(sl, CKB_GAP, 0);
        lv_obj_set_flex_flow(sl, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(sl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(sl, LV_OBJ_FLAG_SCROLLABLE);
        _ckb_slides[i] = sl;
        if (i < 2) _ckb_build(sl, i); // Solo letras al inicio
    }

    return _ckb_root;
}

// ── API pública ───────────────────────────────────────────────
static void custom_kb_set_ta(lv_obj_t* ta) { _ckb_ta = ta; }
static lv_obj_t* custom_kb_get_ta() { return _ckb_ta; }
static void custom_kb_hide() { if (_ckb_root) lv_obj_add_flag(_ckb_root, LV_OBJ_FLAG_HIDDEN); }
static void custom_kb_show() { if (_ckb_root) lv_obj_clear_flag(_ckb_root, LV_OBJ_FLAG_HIDDEN); }
static void custom_kb_reset() {
    // Invalida referencias al cambiar de pantalla
    _ckb_root = nullptr;
    _ckb_cont = nullptr;
    _ckb_ind  = nullptr;
    _ckb_ta   = nullptr;
    for (int i = 0; i < 6; i++) _ckb_slides[i] = nullptr;
    _ckb_slide = 0;
    _ckb_shift = 0;
}
