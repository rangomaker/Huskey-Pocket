// ============================================================
//  ui_toast.h — mensajes breves de confirmación (toasts)
// ------------------------------------------------------------
//  Un toast es un mensaje corto (máx ~3 palabras) que aparece
//  en la parte inferior de la pantalla, se muestra ~2 s y se
//  desvanece solo. Vive en lv_layer_top() para sobrevivir a los
//  cambios de pantalla (guardar/eliminar navegan justo después).
//
//  Uso:
//     toast_ok("Entrada guardada");     // verde pastel
//     toast_del("Carpeta eliminada");   // naranja pastel
//     toast_err("Ya existe");           // rojo pastel
//  o genérico:
//     show_toast("texto", TOAST_OK);
// ============================================================
#pragma once

enum ToastType { TOAST_OK, TOAST_DEL, TOAST_ERR };

static lv_obj_t*  _toast_obj   = nullptr;
static lv_timer_t* _toast_timer = nullptr;
static lv_timer_t* _toast_kill_timer = nullptr; // respaldo de destrucción del fade-out

// Colores pastel suaves (no saturados). Fondo claro + texto oscuro a juego
// + borde del mismo tono pero más intenso/saturado.
static void _toast_colors(ToastType type, lv_color_t* bg, lv_color_t* fg, lv_color_t* bd) {
    switch (type) {
        case TOAST_OK:  *bg = lv_color_hex(0xC9E9D2); *fg = lv_color_hex(0x1E5631); *bd = lv_color_hex(0x3DA35D); break; // verde
        case TOAST_DEL: *bg = lv_color_hex(0xFAD9B8); *fg = lv_color_hex(0x8A4B10); *bd = lv_color_hex(0xE8871E); break; // naranja
        case TOAST_ERR: *bg = lv_color_hex(0xF6C6C6); *fg = lv_color_hex(0x8A1C1C); *bd = lv_color_hex(0xD64545); break; // rojo
        default:        *bg = lv_color_hex(0xDDDDDD); *fg = lv_color_hex(0x333333); *bd = lv_color_hex(0x999999); break;
    }
}

static void _toast_destroy_cb(lv_timer_t* t) {
    if (_toast_obj && lv_obj_is_valid(_toast_obj)) lv_obj_del(_toast_obj);
    _toast_obj = nullptr;
    _toast_timer = nullptr;
    _toast_kill_timer = nullptr;
}

// Animación por traslación vertical
static void _toast_anim_y(void* obj, int32_t v) {
    lv_obj_set_y((lv_obj_t*)obj, v);
}
static int32_t _toast_y_final = 0; // y de reposo, calculada tras el align

// Al terminar el fade-out, destruir el objeto.
static void _toast_fade_done_cb(lv_anim_t* a) {
    if (_toast_obj && lv_obj_is_valid(_toast_obj)) lv_obj_del(_toast_obj);
    _toast_obj = nullptr;
}

static void _toast_start_fadeout(lv_timer_t* tmr) {
    if (_toast_timer) { lv_timer_del(_toast_timer); _toast_timer = nullptr; }
    if (!_toast_obj || !lv_obj_is_valid(_toast_obj)) return;
    // Salida: deslizar hacia abajo hasta fuera de la pantalla.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _toast_obj);
    lv_anim_set_exec_cb(&a, _toast_anim_y);
    lv_anim_set_values(&a, lv_obj_get_y(_toast_obj), UI_H + 4);
    lv_anim_set_time(&a, 280);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&a, _toast_fade_done_cb);
    lv_anim_start(&a);
    // Timer de seguridad para garantizar destrucción del objeto
    lv_timer_t* kill = lv_timer_create(_toast_destroy_cb, 550, nullptr);
    lv_timer_set_repeat_count(kill, 1);
    _toast_kill_timer = kill;
}

void show_toast(const char* text, ToastType type) {
    if (!text) return;
    // Si ya hay un toast en pantalla, reemplazarlo limpiamente.
    if (_toast_timer) { lv_timer_del(_toast_timer); _toast_timer = nullptr; }
    if (_toast_kill_timer) { lv_timer_del(_toast_kill_timer); _toast_kill_timer = nullptr; }
    if (_toast_obj && lv_obj_is_valid(_toast_obj)) { lv_obj_del(_toast_obj); _toast_obj = nullptr; }

    lv_color_t bg, fg, bd;
    _toast_colors(type, &bg, &fg, &bd);

    lv_obj_t* top = lv_layer_top();
    lv_obj_t* t = lv_obj_create(top);
    _toast_obj = t;
    lv_obj_remove_style_all(t);
    lv_obj_set_style_bg_color(t, bg, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(t, 14, 0);
    lv_obj_set_style_pad_hor(t, 18, 0);
    lv_obj_set_style_pad_ver(t, 9, 0);
    lv_obj_set_style_border_width(t, 2, 0);
    lv_obj_set_style_border_color(t, bd, 0);
    lv_obj_set_style_border_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(t, 10, 0);
    lv_obj_set_style_shadow_opa(t, LV_OPA_20, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);
    // Dimensiones ajustadas al contenido
    lv_obj_set_height(t, LV_SIZE_CONTENT);
    lv_obj_set_width(t, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(t, UI_W - 24, 0);

    lv_obj_t* lbl = lv_label_create(t);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, fg, 0);
    lv_obj_set_style_text_font(lbl, FONT_SM, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);

    // Posición fija centrada abajo
    lv_obj_update_layout(t);
    lv_coord_t tw = lv_obj_get_width(t);
    lv_coord_t th = lv_obj_get_height(t);
    _toast_y_final = UI_H - th - 20;
    lv_obj_set_pos(t, (UI_W - tw) / 2, UI_H + 4); // nace bajo el borde inferior
    lv_obj_move_foreground(t);

    lv_anim_t ai;
    lv_anim_init(&ai);
    lv_anim_set_var(&ai, t);
    lv_anim_set_exec_cb(&ai, _toast_anim_y);
    lv_anim_set_values(&ai, UI_H + 4, _toast_y_final);
    lv_anim_set_time(&ai, 260);
    lv_anim_set_delay(&ai, 60); // dejar pasar el primer frame pesado tras un cambio de pantalla
    lv_anim_set_path_cb(&ai, lv_anim_path_ease_out);
    lv_anim_start(&ai);

    // Tras ~1.8 s, iniciar el fade-out.
    _toast_timer = lv_timer_create(_toast_start_fadeout, 1800, nullptr);
    lv_timer_set_repeat_count(_toast_timer, 1);
}

// Atajos semánticos
static inline void toast_ok(const char* text)  { show_toast(text, TOAST_OK); }
static inline void toast_del(const char* text) { show_toast(text, TOAST_DEL); }
static inline void toast_err(const char* text) { show_toast(text, TOAST_ERR); }
