#pragma once
// =============================================================
//  onboarding.h — Flujo de primer uso (bienvenida -> PIN -> ajustes)
//  Reescrito tras encontrar varios problemas en la v1: crashes por
//  borrar la pantalla actual desde su propio evento de clic, PIN con
//  estilo distinto al real, teclado numérico desbordado, texto
//  cortado en tarjetas, fecha/hora incómoda. Esta versión:
//  - Copia las dimensiones EXACTAS del teclado de PIN de pin_screen.h
//  - Las tarjetas seleccionables se actualizan EN SU LUGAR (sin
//    recrear la pantalla) — evita el crash de borrar-desde-su-propio-
//    evento por completo en esos casos.
//  - El cambio de tema sí recrea la pantalla (cambia todos los
//    colores), pero diferido con lv_async_call — igual que la
//    pantalla de sesión PWA.
//  - Fecha y hora separadas en dos pasos, botones +/- más grandes.
// =============================================================
#include <lvgl.h>
#include "vault.h"
#include "rtc_manager.h"
#include "fonts.h"
#include "i18n.h"

#define OB_DEBUG_SKIP 0 // Selector de salto rápido para pruebas internas
#define OB_TOTAL_STEPS 6      // para los puntos de progreso — solo la fase de configuración
#define OB_DEBUG_STEP_COUNT 11 // total real de pasos (incluye Idioma/PIN/progreso/listo)
#define OB_PIN_DIGITS PIN_DIGITS // de vault.h — único punto de cambio

// 0=Idioma 1=Bienvenida 2=Fecha 3=Hora 4=Teclado 5=Auto-bloqueo
// 6=Tema 7=CrearPIN 8=ConfirmarPIN 9=Progreso 10=Listo
static int  _ob_step = 0;
static bool _ob_legal_accepted = false; // lectura del aviso en esta sesion
static bool _ob_active_flag = false;
bool onboarding_in_progress() { return _ob_active_flag; }
static int  _ob_year = 2026, _ob_month = 6, _ob_day = 21, _ob_hour = 12, _ob_minute = 0;
static int  _ob_kb_layout    = 0;
static bool _ob_kb_t9        = false;
static int  _ob_lock_minutes = -1;
static int  _ob_theme        = 0;
static bool _ob_time_24h     = true;

static char _ob_pin_first[OB_PIN_DIGITS + 1] = {0};
static char _ob_pin_buf[OB_PIN_DIGITS + 1]   = {0};
static int  _ob_pin_len          = 0;
static bool _ob_pin_confirm_mode = false;

static void _ob_goto(int step);
static void _ob_goto_async(int step) { lv_async_call([](void* d){ _ob_goto((int)(intptr_t)d); }, (void*)(intptr_t)step); }

// ── Helpers visuales compartidos ───────────────────────────────
static lv_obj_t* _ob_title(lv_obj_t* scr, const char* text, int y) {
    lv_obj_t* t = lv_label_create(scr);
    lv_label_set_text(t, text);
    lv_obj_set_style_text_color(t, C_TEXT, 0);
    lv_obj_set_style_text_font(t, FONT_HDR_LG, 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, UI_W - 40);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, y);
    return t;
}

static lv_obj_t* _ob_subtitle(lv_obj_t* scr, const char* text, int y) {
    lv_obj_t* s = lv_label_create(scr);
    lv_label_set_text(s, text);
    lv_obj_set_style_text_color(s, C_MUTED, 0);
    lv_obj_set_style_text_font(s, FONT_SM, 0);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s, UI_W - 48);
    lv_obj_align(s, LV_ALIGN_TOP_MID, 0, y);
    return s;
}

static void _ob_progress_dots(lv_obj_t* scr, int current) {
    lv_obj_t* wrap = lv_obj_create(scr);
    lv_obj_remove_style_all(wrap);
    lv_obj_set_size(wrap, UI_W, 8);
    lv_obj_align(wrap, LV_ALIGN_BOTTOM_MID, 0, -84);
    lv_obj_clear_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);
    int dot_w = 5, gap = 4, total = OB_TOTAL_STEPS;
    int total_w = total * dot_w + (total - 1) * gap;
    int x = (UI_W - total_w) / 2;
    for (int i = 0; i < total; i++) {
        lv_obj_t* d = lv_obj_create(wrap);
        lv_obj_remove_style_all(d);
        bool on = (i == current);
        lv_obj_set_size(d, on ? 13 : dot_w, dot_w);
        lv_obj_set_pos(d, x, 0);
        lv_obj_set_style_radius(d, 3, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(d, on ? C_ACCENT : C_BORDER, 0);
        x += (on ? 13 : dot_w) + gap;
    }
}

static lv_obj_t* _ob_next_btn(lv_obj_t* scr, const char* label, void (*cb)(lv_event_t*)) {
    lv_obj_t* btn = lv_btn_create(scr);
    lv_obj_set_size(btn, UI_W - 64, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    _press_fb(btn);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, C_ACCENT_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_MD, 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    return btn;
}

// Botón para volver al paso anterior por si el usuario se equivocó.
// No se muestra en la pantalla de bienvenida (no hay paso anterior).
static void _ob_add_back_btn(lv_obj_t* scr, int prev_step) {
    lv_obj_t* btn = lv_obj_create(scr);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 48, 48);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_t* ico = lv_label_create(btn);
    lv_label_set_text(ico, ICO_ARROW_BACK);
    lv_obj_set_style_text_font(ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(ico, C_MUTED, 0);
    lv_obj_center(ico);
    lv_obj_add_event_cb(btn, [](lv_event_t* e){
        int step = (int)(intptr_t)lv_event_get_user_data(e);
        _ob_goto_async(step);
    }, LV_EVENT_CLICKED, (void*)(intptr_t)prev_step);
}

#if OB_DEBUG_SKIP
static void _ob_add_debug_skip(lv_obj_t* scr) {
    lv_obj_t* btn = lv_obj_create(scr);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 34, 34);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_t* ico = lv_label_create(btn);
    lv_label_set_text(ico, ICO_SETTINGS);
    lv_obj_set_style_text_font(ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(ico, C_MUTED, 0);
    lv_obj_center(ico);
    lv_obj_add_event_cb(btn, [](lv_event_t* e){
        lv_obj_t* top = lv_layer_top();
        lv_obj_t* backdrop = lv_obj_create(top);
        lv_obj_set_size(backdrop, UI_W, UI_H);
        lv_obj_set_pos(backdrop, 0, 0);
        lv_obj_set_style_bg_color(backdrop, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
        lv_obj_set_style_border_width(backdrop, 0, 0);
        lv_obj_set_style_radius(backdrop, 0, 0);
        lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* pop = lv_obj_create(backdrop);
        lv_obj_set_size(pop, UI_W - 60, 250);
        lv_obj_center(pop);
        lv_obj_set_style_radius(pop, 14, 0);
        lv_obj_set_style_bg_color(pop, C_CARD, 0);
        lv_obj_set_style_bg_opa(pop, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(pop, 0, 0);
        lv_obj_clear_flag(pop, LV_OBJ_FLAG_SCROLLABLE);
        const char* names[OB_DEBUG_STEP_COUNT] = {"Idioma","Bienvenida",t(K_FECHA),t(K_HORA),"Teclado","Auto-bloqueo",
                                                    "Tema","Crear PIN","Confirmar PIN","Progreso","Listo"};
        for (int i = 0; i < OB_DEBUG_STEP_COUNT; i++) {
            lv_obj_t* b = lv_btn_create(pop);
            lv_obj_set_size(b, UI_W - 90, 18);
            lv_obj_set_pos(b, 0, i * 20);
            lv_obj_set_style_bg_color(b, C_BG, 0);
            lv_obj_set_style_radius(b, 6, 0);
            lv_obj_set_style_border_width(b, 0, 0);
            lv_obj_t* l = lv_label_create(b);
            lv_label_set_text(l, names[i]);
            lv_obj_set_style_text_font(l, FONT_SM, 0);
            lv_obj_set_style_text_color(l, C_TEXT, 0);
            lv_obj_center(l);
            lv_obj_add_event_cb(b, [](lv_event_t* e){
                int step = (int)(intptr_t)lv_event_get_user_data(e);
                lv_async_call([](void* d){
                    lv_obj_clean(lv_layer_top());
                    _ob_goto((int)(intptr_t)d);
                }, (void*)(intptr_t)step);
            }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
        lv_obj_add_event_cb(backdrop, [](lv_event_t* e){
            lv_async_call([](void*){ lv_obj_clean(lv_layer_top()); }, nullptr);
        }, LV_EVENT_CLICKED, nullptr);
    }, LV_EVENT_CLICKED, nullptr);
}
#else
static void _ob_add_debug_skip(lv_obj_t* scr) {}
#endif

// ── Paso 0: Idioma — primera pantalla, antes que todo lo demás ──
static lv_obj_t* _ob_lang_cards[LANG_COUNT];
static lv_obj_t* _ob_lang_checks[LANG_COUNT];

static void _ob_lang_select(int idx) {
    i18n_set_lang(idx);
    _ob_goto_async(0); // recarga la pantalla completa ya en el idioma elegido
}

static void _show_ob_language() {
    lv_obj_t* scr = _make_screen();
    _ob_add_debug_skip(scr);
    // Sin botón de "atrás" — es la primera pantalla del flujo.
    _ob_title(scr, t(K_SELECT_LANG), 64);

    const char* names[LANG_COUNT] = { "Español", "English" };
    int cur = i18n_get_lang();
    for (int i = 0; i < LANG_COUNT; i++) {
        lv_obj_t* card = lv_obj_create(scr);
        lv_obj_set_size(card, UI_W - 48, 44);
        lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 132 + i * 50);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_bg_color(card, C_CARD, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, (cur == i) ? 2 : 0, 0);
        lv_obj_set_style_border_color(card, C_ACCENT, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        _press_fb(card);
        _ob_lang_cards[i] = card;

        lv_obj_t* lbl = lv_label_create(card);
        lv_label_set_text(lbl, names[i]);
        lv_obj_set_style_text_color(lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl, FONT_MD, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 14, 0);

        lv_obj_t* chk = lv_label_create(card);
        lv_label_set_text(chk, ICO_CHECK_CIRCLE);
        lv_obj_set_style_text_font(chk, FONT_ICON_EX, 0);
        lv_obj_set_style_text_color(chk, C_ACCENT, 0);
        lv_obj_align(chk, LV_ALIGN_RIGHT_MID, -12, 0);
        if (cur != i) lv_obj_add_flag(chk, LV_OBJ_FLAG_HIDDEN);
        _ob_lang_checks[i] = chk;

        lv_obj_add_event_cb(card, [](lv_event_t* e){
            int idx = (int)(intptr_t)lv_event_get_user_data(e);
            _ob_lang_select(idx);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    _ob_next_btn(scr, t(K_CONTINUAR), [](lv_event_t* e){ _ob_goto(1); });
    _load_screen_instant(scr);
}

// ── Paso 1: Bienvenida ──────────────────────────────────────────
static void _show_ob_welcome() {
    lv_obj_t* scr = _make_screen();
    _ob_add_debug_skip(scr);
    _ob_add_back_btn(scr, 0);

    lv_obj_t* icon = lv_label_create(scr);
    lv_label_set_text(icon, ICO_SHIELD);
    lv_obj_set_style_text_font(icon, FONT_ICON, 0);
    lv_obj_set_style_text_color(icon, C_ACCENT, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -80);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "Huskey Pocket");
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_HDR_LG, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, UI_W - 40);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -34);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, t(K_OB_TAGLINE));
    lv_obj_set_style_text_color(sub, C_MUTED, 0);
    lv_obj_set_style_text_font(sub, FONT_SM, 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(sub, UI_W - 48);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 4);

    _ob_next_btn(scr, t(K_COMENZAR), [](lv_event_t* e){
        _ob_goto(_ob_legal_accepted ? 2 : 11); // aviso antes de configurar
    });
    _load_screen_instant(scr);
}

// ── Pasos 1 y 2: Fecha / Hora (ruedas nativas — más fácil de tocar
//    con precisión que botones +/- chicos) ──────────────────────
#define OB_YEAR_MIN 2026
#define OB_YEAR_MAX 2100

static lv_obj_t* _ob_roller[3]; // hasta 3 ruedas por pantalla

// Opciones numéricas para lv_roller con verificación de capacidad por entrada
static void _ob_build_num_options(char* buf, size_t buf_len, int from, int to, int width) {
    size_t pos = 0;
    for (int v = from; v <= to; v++) {
        size_t need = (size_t)width + 2; // número + posible '\n' + '\0' final
        if (pos + need > buf_len) break; // no entra completa -> no se corta a la mitad
        int n = snprintf(buf + pos, buf_len - pos, width == 4 ? "%04d" : "%02d", v);
        pos += n;
        if (v < to) { buf[pos++] = '\n'; }
    }
    buf[pos] = '\0';
}

static lv_obj_t* _ob_make_roller(lv_obj_t* scr, int x, int y, int w, int h,
                                  const char* options, int selected_idx, lv_roller_mode_t mode) {
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
    lv_roller_set_selected(r, selected_idx, LV_ANIM_OFF);
    return r;
}

static int _ob_days_in_month(int month, int year) {
    static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) return 29;
    return d[month - 1];
}

static void _show_ob_date() {
    lv_obj_t* scr = _make_screen();
    _ob_add_debug_skip(scr);
    _ob_add_back_btn(scr, 1);
    _ob_title(scr, t(K_FECHA), 28);

    int rw = 70, gap = 8, rh = 96;
    int total_w = 3 * rw + 2 * gap;
    int x0 = (UI_W - total_w) / 2;
    int y0 = 100;

    static char year_opts[420], month_opts[64], day_opts[100];
    _ob_build_num_options(year_opts, sizeof(year_opts), OB_YEAR_MIN, OB_YEAR_MAX, 4);
    _ob_build_num_options(month_opts, sizeof(month_opts), 1, 12, 2);
    _ob_build_num_options(day_opts, sizeof(day_opts), 1, 31, 2);

    if (_ob_year < OB_YEAR_MIN) _ob_year = OB_YEAR_MIN;
    if (_ob_year > OB_YEAR_MAX) _ob_year = OB_YEAR_MAX;

    _ob_roller[0] = _ob_make_roller(scr, x0,                  y0, rw, rh, year_opts,  _ob_year - OB_YEAR_MIN, LV_ROLLER_MODE_NORMAL);
    _ob_roller[1] = _ob_make_roller(scr, x0 + rw + gap,        y0, rw, rh, month_opts, _ob_month - 1,          LV_ROLLER_MODE_INFINITE);
    _ob_roller[2] = _ob_make_roller(scr, x0 + 2 * (rw + gap),  y0, rw, rh, day_opts,   _ob_day - 1,            LV_ROLLER_MODE_INFINITE);

    auto _ob_refresh_day_roller = [](lv_event_t* e){
        int y = OB_YEAR_MIN + lv_roller_get_selected(_ob_roller[0]);
        int m = 1 + lv_roller_get_selected(_ob_roller[1]);
        int maxd = _ob_days_in_month(m, y);
        static char opts[100];
        _ob_build_num_options(opts, sizeof(opts), 1, maxd, 2);
        int cur_day = 1 + lv_roller_get_selected(_ob_roller[2]);
        if (cur_day > maxd) cur_day = maxd;
        lv_roller_set_options(_ob_roller[2], opts, LV_ROLLER_MODE_INFINITE);
        lv_roller_set_selected(_ob_roller[2], cur_day - 1, LV_ANIM_OFF);
    };
    lv_obj_add_event_cb(_ob_roller[0], _ob_refresh_day_roller, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(_ob_roller[1], _ob_refresh_day_roller, LV_EVENT_VALUE_CHANGED, nullptr);

    _ob_progress_dots(scr, 1);
    _ob_next_btn(scr, t(K_SIGUIENTE), [](lv_event_t* e){
        _ob_year  = OB_YEAR_MIN + lv_roller_get_selected(_ob_roller[0]);
        _ob_month = 1 + lv_roller_get_selected(_ob_roller[1]);
        _ob_day   = 1 + lv_roller_get_selected(_ob_roller[2]);
        int maxd = _ob_days_in_month(_ob_month, _ob_year);
        if (_ob_day > maxd) _ob_day = maxd; // por si se eligió un día que no existe en ese mes
        _ob_goto(3);
    });
    _load_screen_instant(scr);
}

static void _ob_hour24_to_12(int h24, int* h12, bool* pm) {
    *pm = (h24 >= 12);
    int h = h24 % 12;
    if (h == 0) h = 12;
    *h12 = h;
}
static int _ob_hour12_to_24(int h12, bool pm) {
    int h = h12 % 12;
    if (pm) h += 12;
    return h;
}

static void _show_ob_time() {
    lv_obj_t* scr = _make_screen();
    _ob_add_debug_skip(scr);
    _ob_add_back_btn(scr, 2);
    _ob_title(scr, t(K_HORA), 26);

    // Selector de formato — dos botones tipo pestaña
    int seg_w = 70, seg_h = 28, seg_gap = 4;
    int seg_x0 = (UI_W - (2 * seg_w + seg_gap)) / 2;
    int seg_y = 56;
    const char* seg_labels[2] = {t(K_24HORAS), t(K_12HORAS)};
    for (int i = 0; i < 2; i++) {
        bool sel = (i == 0) == _ob_time_24h;
        lv_obj_t* b = lv_btn_create(scr);
        lv_obj_set_size(b, seg_w, seg_h);
        lv_obj_set_pos(b, seg_x0 + i * (seg_w + seg_gap), seg_y);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_bg_color(b, sel ? C_ACCENT : C_CARD, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, seg_labels[i]);
        lv_obj_set_style_text_color(l, sel ? lv_color_hex(0xFFFFFF) : C_TEXT, 0);
        lv_obj_set_style_text_font(l, FONT_SM, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, [](lv_event_t* e){
            bool to24 = (bool)(intptr_t)lv_event_get_user_data(e);
            if (to24 == _ob_time_24h) return; // ya está en ese modo

            // Leer la selección ACTUAL antes de reconstruir la pantalla con
            // el otro formato — si no, se pierde y vuelve al valor por
            // defecto (12:00).
            if (_ob_time_24h) {
                _ob_hour   = lv_roller_get_selected(_ob_roller[0]);
                _ob_minute = lv_roller_get_selected(_ob_roller[1]);
            } else {
                int h12 = 1 + lv_roller_get_selected(_ob_roller[0]);
                _ob_minute = lv_roller_get_selected(_ob_roller[1]);
                bool pm = lv_roller_get_selected(_ob_roller[2]) == 1;
                _ob_hour = _ob_hour12_to_24(h12, pm);
            }
            _ob_time_24h = to24;
            _ob_goto_async(3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)(i == 0));
    }

    int y0 = 96, rh = 96;
    if (_ob_time_24h) {
        int rw = 80, gap = 16;
        int x0 = (UI_W - (2 * rw + gap)) / 2;
        static char hour_opts[80], min_opts[200];
        _ob_build_num_options(hour_opts, sizeof(hour_opts), 0, 23, 2);
        _ob_build_num_options(min_opts, sizeof(min_opts), 0, 59, 2);
        _ob_roller[0] = _ob_make_roller(scr, x0,            y0, rw, rh, hour_opts, _ob_hour,   LV_ROLLER_MODE_INFINITE);
        _ob_roller[1] = _ob_make_roller(scr, x0 + rw + gap, y0, rw, rh, min_opts,  _ob_minute, LV_ROLLER_MODE_INFINITE);
    } else {
        int rw = 56, gap = 8;
        int x0 = (UI_W - (3 * rw + 2 * gap)) / 2;
        int h12; bool pm;
        _ob_hour24_to_12(_ob_hour, &h12, &pm);
        static char hour12_opts[64], min_opts[200];
        static const char ampm_opts[] = "AM\nPM";
        _ob_build_num_options(hour12_opts, sizeof(hour12_opts), 1, 12, 2);
        _ob_build_num_options(min_opts, sizeof(min_opts), 0, 59, 2);
        _ob_roller[0] = _ob_make_roller(scr, x0,                  y0, rw, rh, hour12_opts, h12 - 1,     LV_ROLLER_MODE_INFINITE);
        _ob_roller[1] = _ob_make_roller(scr, x0 + rw + gap,        y0, rw, rh, min_opts,    _ob_minute,  LV_ROLLER_MODE_INFINITE);
        _ob_roller[2] = _ob_make_roller(scr, x0 + 2 * (rw + gap),  y0, rw, rh, ampm_opts,   pm ? 1 : 0,  LV_ROLLER_MODE_NORMAL);
    }

    _ob_progress_dots(scr, 2);
    _ob_next_btn(scr, t(K_SIGUIENTE), [](lv_event_t* e){
        if (_ob_time_24h) {
            _ob_hour   = lv_roller_get_selected(_ob_roller[0]);
            _ob_minute = lv_roller_get_selected(_ob_roller[1]);
        } else {
            int h12 = 1 + lv_roller_get_selected(_ob_roller[0]);
            _ob_minute = lv_roller_get_selected(_ob_roller[1]);
            bool pm = lv_roller_get_selected(_ob_roller[2]) == 1;
            _ob_hour = _ob_hour12_to_24(h12, pm);
        }
        rtc_set(_ob_year, _ob_month, _ob_day, _ob_hour, _ob_minute, 0);
        _ob_goto(4);
    });
    _load_screen_instant(scr);
}

// ── Pasos 3 y 4: Crear / Confirmar PIN ──────────────────────────
// Mismas dimensiones EXACTAS que pin_screen.h para que se vea como
// la pantalla de PIN real, no como un widget distinto.
static const char* _ob_kstr[] = { "1","2","3","4","5","6","7","8","9","DEL","0","OK" };
static lv_obj_t* _ob_pin_dots[OB_PIN_DIGITS];
static lv_obj_t* _ob_pin_status_lbl = nullptr;

static void _ob_pin_update_dots() {
    for (int i = 0; i < OB_PIN_DIGITS; i++) {
        bool filled = i < _ob_pin_len;
        lv_obj_set_style_bg_color(_ob_pin_dots[i], filled ? C_ACCENT : C_BG, 0);
        lv_obj_set_style_bg_opa(_ob_pin_dots[i], filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

static void _ob_pin_try_confirm() {
    if (!_ob_pin_confirm_mode) {
        strncpy(_ob_pin_first, _ob_pin_buf, OB_PIN_DIGITS);
        _ob_pin_first[OB_PIN_DIGITS] = '\0';
        _ob_goto_async(8);
    } else {
        if (strcmp(_ob_pin_buf, _ob_pin_first) == 0) {
            _ob_goto_async(9); // pantalla de progreso — ahí se llama vault_init() por pasos
        } else {
            if (_ob_pin_status_lbl) lv_label_set_text(_ob_pin_status_lbl, t(K_NO_COINCIDEN));
            _ob_pin_len = 0; _ob_pin_buf[0] = '\0';
            _ob_pin_update_dots();
        }
    }
}

static void _ob_pin_key_cb(lv_event_t* e) {
    const char* k = (const char*)lv_event_get_user_data(e);
    if (!k) return;
    if (k[0] == 'D') {
        if (_ob_pin_len > 0) { _ob_pin_buf[--_ob_pin_len] = '\0'; _ob_pin_update_dots(); }
    } else if (k[0] == 'O') {
        if (_ob_pin_len == OB_PIN_DIGITS) _ob_pin_try_confirm();
    } else {
        if (_ob_pin_len < OB_PIN_DIGITS) {
            _ob_pin_buf[_ob_pin_len++] = k[0];
            _ob_pin_buf[_ob_pin_len] = '\0';
            _ob_pin_update_dots();
            if (_ob_pin_status_lbl) lv_label_set_text(_ob_pin_status_lbl, "");
            if (_ob_pin_len == OB_PIN_DIGITS) _ob_pin_try_confirm();
        }
    }
}

static void _ob_build_pin_screen(const char* title, const char* subtitle, bool confirm_mode) {
    lv_obj_t* scr = _make_screen();
    _ob_add_debug_skip(scr);
    _ob_add_back_btn(scr, confirm_mode ? 7 : 6); // confirmar -> vuelve a crear; crear -> vuelve a tema
    _ob_pin_confirm_mode = confirm_mode;
    _ob_pin_len = 0; _ob_pin_buf[0] = '\0';

    lv_obj_t* t = lv_label_create(scr);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, C_TEXT, 0);
    lv_obj_set_style_text_font(t, FONT_HDR, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, subtitle);
    lv_obj_set_style_text_color(sub, C_MUTED, 0);
    lv_obj_set_style_text_font(sub, FONT_SM, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 62);

    // Dots — mismas dimensiones que pin_screen.h
    int dot_d = 14, dot_gap = 10;
    int dot_tot = OB_PIN_DIGITS * dot_d + (OB_PIN_DIGITS - 1) * dot_gap;
    int dot_x0 = (UI_W - dot_tot) / 2;
    int dot_y = 88;
    for (int i = 0; i < OB_PIN_DIGITS; i++) {
        lv_obj_t* d = lv_obj_create(scr);
        lv_obj_set_size(d, dot_d, dot_d);
        lv_obj_set_pos(d, dot_x0 + i * (dot_d + dot_gap), dot_y);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(d, 2, 0);
        lv_obj_set_style_border_color(d, C_ACCENT, 0);
        lv_obj_set_style_pad_all(d, 0, 0);
        _ob_pin_dots[i] = d;
    }
    _ob_pin_update_dots();

    _ob_pin_status_lbl = lv_label_create(scr);
    lv_label_set_text(_ob_pin_status_lbl, "");
    lv_obj_set_style_text_color(_ob_pin_status_lbl, C_DANGER, 0);
    lv_obj_set_style_text_font(_ob_pin_status_lbl, FONT_SM, 0);
    lv_obj_align(_ob_pin_status_lbl, LV_ALIGN_TOP_MID, 0, dot_y + dot_d + 6);

    // Grid 3x4 — key_d=43, gap=8, gy=120 (idéntico a pin_screen.h)
    int key_d = 43, gap = 8;
    int gw = 3 * key_d + 2 * gap;
    int gx = (UI_W - gw) / 2;
    int gy = 120;
    for (int i = 0; i < 12; i++) {
        int col = i % 3, row = i / 3;
        int x = gx + col * (key_d + gap);
        int y = gy + row * (key_d + gap);
        bool is_del = (i == 9), is_ok = (i == 11);

        lv_obj_t* btn = lv_btn_create(scr);
        lv_obj_set_size(btn, key_d, key_d);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, C_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, C_ACCENT, LV_STATE_PRESSED);

        lv_obj_t* lbl = lv_label_create(btn);
        if (is_del) {
            lv_label_set_text(lbl, ICO_BACKSPACE);
            lv_obj_set_style_text_color(lbl, C_TEXT, 0);
            lv_obj_set_style_text_font(lbl, FONT_ICON, 0);
        } else if (is_ok) {
            lv_label_set_text(lbl, ICO_CHECK);
            lv_obj_set_style_text_color(lbl, C_ACCENT, 0);
            lv_obj_set_style_text_font(lbl, FONT_ICON, 0);
        } else {
            lv_label_set_text(lbl, _ob_kstr[i]);
            lv_obj_set_style_text_color(lbl, C_TEXT, 0);
            lv_obj_set_style_text_font(lbl, FONT_HDR_LG, 0);
        }
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, _ob_pin_key_cb, LV_EVENT_CLICKED, (void*)_ob_kstr[i]);
    }

    // Sin puntos de progreso aquí — el teclado ocupa casi todo el alto
    // de la pantalla (hasta y=316), no queda espacio libre sin que se
    // superpongan.
    _load_screen_instant(scr);
}

static void _show_ob_pin_create()  { _ob_build_pin_screen(t(K_CREA_TU_PIN), t(K_LO_USARAS), false); }
static void _show_ob_pin_confirm() { _ob_build_pin_screen(t(K_CONFIRMALO), t(K_VUELVE_ESCRIBIR_PIN), true); }

// ── Paso 5: Idioma de entrada + estilo de teclado ────────────
static lv_obj_t* _ob_kb_lang_btns[3];
static lv_obj_t* _ob_kb_style_cards[2];
static lv_obj_t* _ob_kb_style_checks[2];

static void _ob_kb_lang_select(int idx) {
    _ob_kb_layout = idx;
    const char* lang_labels[3] = {"Lat MX", "Es ES", "En US"};
    for (int i = 0; i < 3; i++) {
        bool sel = (i == idx);
        lv_obj_set_style_bg_color(_ob_kb_lang_btns[i], sel ? C_ACCENT : C_CARD, 0);
        lv_obj_t* l = lv_obj_get_child(_ob_kb_lang_btns[i], 0);
        if (l) lv_obj_set_style_text_color(l, sel ? C_ACCENT_TEXT : C_TEXT, 0);
    }
}

static void _ob_kb_style_select(int idx) {
    _ob_kb_t9 = (idx == 1);
    for (int i = 0; i < 2; i++) {
        bool sel = (i == idx);
        lv_obj_set_style_border_width(_ob_kb_style_cards[i], sel ? 2 : 0, 0);
        if (sel) lv_obj_clear_flag(_ob_kb_style_checks[i], LV_OBJ_FLAG_HIDDEN);
        else     lv_obj_add_flag(_ob_kb_style_checks[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void _show_ob_keyboard() {
    lv_obj_t* scr = _make_screen();
    _ob_add_debug_skip(scr);
    _ob_add_back_btn(scr, 3);

    // Título largo → FONT_HDR más pequeño
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, t(K_IDIOMA_ESTILO_TECLADO));
    lv_obj_set_style_text_font(title, FONT_HDR, 0);
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, UI_W - 32);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 42);

    // ── 3 botones de idioma en fila ──────────────────────────
    const char* lang_labels[3] = {"Lat MX", "Es ES", "En US"};
    const int BW = 70, BH = 30, BGAP = 4;
    int bx0 = (UI_W - (3*BW + 2*BGAP)) / 2;
    for (int i = 0; i < 3; i++) {
        lv_obj_t* btn = lv_obj_create(scr);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(btn, BW, BH);
        lv_obj_set_pos(btn, bx0 + i*(BW+BGAP), 84);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_bg_color(btn, i==_ob_kb_layout ? C_ACCENT : C_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        _ob_kb_lang_btns[i] = btn;
        lv_obj_t* l = lv_label_create(btn);
        lv_label_set_text(l, lang_labels[i]);
        lv_obj_set_style_text_font(l, FONT_SM, 0);
        lv_obj_set_style_text_color(l, i==_ob_kb_layout ? C_ACCENT_TEXT : C_TEXT, 0);
        lv_obj_center(l);
        lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        _press_fb(btn);
        lv_obj_add_event_cb(btn, [](lv_event_t* e){
            _ob_kb_lang_select((int)(intptr_t)lv_event_get_user_data(e));
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    // ── 2 cards de estilo ────────────────────────────────────
    const char* style_labels[2] = {t(K_KB_DESLIZANTE), t(K_KB_T9)};
    const char* style_desc[2]   = {t(K_KB_DESLIZANTE_DESC), t(K_KB_T9_DESC)};
    for (int i = 0; i < 2; i++) {
        lv_obj_t* card = lv_obj_create(scr);
        lv_obj_set_size(card, UI_W - 48, 63);
        lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 124 + i * 71);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_bg_color(card, C_CARD, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, (!_ob_kb_t9 && i==0)||(   _ob_kb_t9 && i==1) ? 2 : 0, 0);
        lv_obj_set_style_border_color(card, C_ACCENT, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        _press_fb(card);
        _ob_kb_style_cards[i] = card;

        lv_obj_t* lbl = lv_label_create(card);
        lv_label_set_text(lbl, style_labels[i]);
        lv_obj_set_style_text_font(lbl, FONT_HDR, 0);
        lv_obj_set_style_text_color(lbl, C_TEXT, 0);
        lv_obj_set_pos(lbl, 4, -8);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* desc = lv_label_create(card);
        lv_label_set_text(desc, style_desc[i]);
        lv_obj_set_style_text_font(desc, FONT_SM, 0);
        lv_obj_set_style_text_color(desc, C_MUTED, 0);
        lv_obj_set_size(desc, UI_W - 48 - 50, 32);
        lv_obj_set_pos(desc, 4, 10);
        lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
        lv_obj_clear_flag(desc, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* chk = lv_label_create(card);
        lv_label_set_text(chk, ICO_CHECK_CIRCLE);
        lv_obj_set_style_text_font(chk, FONT_ICON_EX, 0);
        lv_obj_set_style_text_color(chk, C_ACCENT, 0);
        lv_obj_align(chk, LV_ALIGN_RIGHT_MID, 0, 0);
        if ((!_ob_kb_t9 && i!=0)||(_ob_kb_t9 && i!=1)) lv_obj_add_flag(chk, LV_OBJ_FLAG_HIDDEN);
        _ob_kb_style_checks[i] = chk;

        lv_obj_add_event_cb(card, [](lv_event_t* e){
            _ob_kb_style_select((int)(intptr_t)lv_event_get_user_data(e));
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    lv_obj_t* next = _ob_next_btn(scr, t(K_SIGUIENTE), [](lv_event_t* e){
        vault_set_kb_layout(_ob_kb_layout);
        vault_set_t9_mode(_ob_kb_t9);
        _ob_goto(5);
    });
    lv_obj_align(next, LV_ALIGN_TOP_MID, 0, 263);
    _load_screen_instant(scr);
}

// ── Paso 6: Auto-bloqueo (mismo patrón en su lugar) ─────────────
static lv_obj_t* _ob_lock_cards[4];
static lv_obj_t* _ob_lock_checks[4];
static const int _ob_lock_values[4] = {-1, 1, 5, 0};

static void _ob_lock_select(int idx) {
    _ob_lock_minutes = _ob_lock_values[idx];
    for (int i = 0; i < 4; i++) {
        bool sel = (_ob_lock_values[i] == _ob_lock_minutes);
        lv_obj_set_style_border_width(_ob_lock_cards[i], sel ? 2 : 0, 0);
        if (sel) lv_obj_clear_flag(_ob_lock_checks[i], LV_OBJ_FLAG_HIDDEN);
        else     lv_obj_add_flag(_ob_lock_checks[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void _show_ob_locktime() {
    lv_obj_t* scr = _make_screen();
    _ob_add_debug_skip(scr);
    _ob_add_back_btn(scr, 4);
    _ob_title(scr, t(K_BLOQUEO_AUTO), 32);
    // Sin subtítulo aquí — con 4 tarjetas ya no cabía sin superponerse
    // con el botón. El título ya transmite la idea.

    const char* labels[4] = {t(K_30SEGUNDOS), t(K_1MINUTO), t(K_5MINUTOS), t(K_NUNCA)};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* card = lv_obj_create(scr);
        lv_obj_set_size(card, UI_W - 48, 40);
        lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 70 + i * 44);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_bg_color(card, C_CARD, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, (_ob_lock_minutes == _ob_lock_values[i]) ? 2 : 0, 0);
        lv_obj_set_style_border_color(card, C_ACCENT, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        _press_fb(card);
        _ob_lock_cards[i] = card;

        lv_obj_t* lbl = lv_label_create(card);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_color(lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl, FONT_MD, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 14, 0);

        lv_obj_t* chk = lv_label_create(card);
        lv_label_set_text(chk, ICO_CHECK_CIRCLE);
        lv_obj_set_style_text_font(chk, FONT_ICON_EX, 0);
        lv_obj_set_style_text_color(chk, C_ACCENT, 0);
        lv_obj_align(chk, LV_ALIGN_RIGHT_MID, -12, 0);
        if (_ob_lock_minutes != _ob_lock_values[i]) lv_obj_add_flag(chk, LV_OBJ_FLAG_HIDDEN);
        _ob_lock_checks[i] = chk;

        lv_obj_add_event_cb(card, [](lv_event_t* e){
            int idx = (int)(intptr_t)lv_event_get_user_data(e);
            _ob_lock_select(idx);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    // Sin puntos de progreso — no hay espacio libre sin superponerse
    // (4 tarjetas + botón ya ocupan casi todo el alto disponible).
    _ob_next_btn(scr, t(K_SIGUIENTE), [](lv_event_t* e){
        // No se guarda aquí — la bóveda todavía no existe (el PIN va al
        // final). _ob_lock_minutes ya quedó elegido y se aplica de
        // verdad justo después de crear el PIN, en la pantalla de progreso.
        _ob_goto(6);
    });
    _load_screen_instant(scr);
}

// ── Paso 7: Tema (cambia colores de toda la pantalla -> sí recrea,
//    pero diferido con lv_async_call para no romper nada) ──────
static void _show_ob_theme() {
    lv_obj_t* scr = _make_screen();
    _ob_add_debug_skip(scr);
    _ob_add_back_btn(scr, 5);
    _ob_title(scr, t(K_APARIENCIA), 28);
    _ob_subtitle(scr, t(K_APARIENCIA_SUB), 64);

    struct { const char* name; uint32_t bg, card, accent; } previews[2] = {
        {t(K_OSCURO), 0x1C1C1E, 0x2C2C2E, 0x0A84FF},
        {t(K_CLARO),  0xF5F5F3, 0xFFFFFF, 0x0A84FF},
    };
    int card_w = (UI_W - 48 - 12) / 2;
    for (int i = 0; i < 2; i++) {
        int x = 24 + i * (card_w + 12);
        lv_obj_t* card = lv_obj_create(scr);
        lv_obj_set_size(card, card_w, 120);
        lv_obj_set_pos(card, x, 100);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_bg_color(card, C_CARD, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, (_ob_theme == i) ? 2 : 0, 0);
        lv_obj_set_style_border_color(card, C_ACCENT, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        _press_fb(card);

        lv_obj_t* prev = lv_obj_create(card);
        lv_obj_remove_style_all(prev);
        lv_obj_set_size(prev, card_w - 16, 60);
        lv_obj_set_pos(prev, 8, 8);
        lv_obj_set_style_radius(prev, 8, 0);
        lv_obj_set_style_bg_color(prev, lv_color_hex(previews[i].bg), 0);
        lv_obj_set_style_bg_opa(prev, LV_OPA_COVER, 0);
        lv_obj_clear_flag(prev, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(prev, LV_OBJ_FLAG_CLICKABLE); // si no, capturaba el toque antes que la tarjeta

        lv_obj_t* bar1 = lv_obj_create(prev);
        lv_obj_remove_style_all(bar1);
        lv_obj_set_size(bar1, card_w - 32, 8);
        lv_obj_set_pos(bar1, 8, 10);
        lv_obj_set_style_radius(bar1, 4, 0);
        lv_obj_set_style_bg_color(bar1, lv_color_hex(previews[i].card), 0);
        lv_obj_set_style_bg_opa(bar1, LV_OPA_COVER, 0);
        lv_obj_clear_flag(bar1, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* bar2 = lv_obj_create(prev);
        lv_obj_remove_style_all(bar2);
        lv_obj_set_size(bar2, (int)((card_w - 32) * 0.5f), 8);
        lv_obj_set_pos(bar2, 8, 26);
        lv_obj_set_style_radius(bar2, 4, 0);
        lv_obj_set_style_bg_color(bar2, lv_color_hex(previews[i].accent), 0);
        lv_obj_set_style_bg_opa(bar2, LV_OPA_COVER, 0);
        lv_obj_clear_flag(bar2, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(card);
        lv_label_set_text(lbl, previews[i].name);
        lv_obj_set_style_text_color(lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl, FONT_MD, 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

        lv_obj_add_event_cb(card, [](lv_event_t* e){
            int idx = (int)(intptr_t)lv_event_get_user_data(e);
            _ob_theme = idx;
            vault_set_theme(_ob_theme);
            _apply_theme(_ob_theme);
            _ob_goto_async(6);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    _ob_progress_dots(scr, 5);
    _ob_next_btn(scr, t(K_SIGUIENTE), [](lv_event_t* e){ _ob_goto(7); });
    _load_screen_instant(scr);
}

// ── Paso 8: Progreso (creación del almacenamiento por pasos) ────
static lv_obj_t*  _ob_progress_fill   = nullptr;
static lv_obj_t*  _ob_progress_pctlbl = nullptr;
static lv_timer_t* _ob_progress_timer = nullptr;

static void _ob_breathe_opa_cb(void* var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}

static void _ob_progress_timer_cb(lv_timer_t* timer) {
    bool more = vaultfs_init_step(1); // 1 bucket por paso — bloques más chicos, animación más fluida
    int pct = vaultfs_init_progress_pct();
    if (_ob_progress_fill) lv_obj_set_width(_ob_progress_fill, (UI_W - 80) * pct / 100);
    if (_ob_progress_pctlbl) { char b[8]; snprintf(b, 8, "%d%%", pct); lv_label_set_text(_ob_progress_pctlbl, b); }

    if (!more) {
        lv_timer_del(timer);
        _ob_progress_timer = nullptr;
        vault_init(_ob_pin_first);            // ahora es rápido — los buckets ya existen
        // Borrar el PIN de RAM inmediatamente tras usarlo — igual que
        // pin_screen.h. Sin esto quedaba en claro en estáticos durante
        // toda la sesión.
        memset(_ob_pin_first, 0, sizeof(_ob_pin_first));
        memset(_ob_pin_buf,   0, sizeof(_ob_pin_buf));
        _ob_pin_len = 0;
        vault_set_lock_minutes(_ob_lock_minutes); // recién aquí hay bóveda lista para guardar esto
        _ob_goto(10);
    }
}

static void _show_ob_progress() {
    lv_obj_t* scr = _make_screen();

    lv_obj_t* icon = lv_label_create(scr);
    lv_label_set_text(icon, ICO_LOCK);
    lv_obj_set_style_text_font(icon, FONT_ICON, 0);
    lv_obj_set_style_text_color(icon, C_ACCENT, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -94);

    // Único elemento que "respira" — el resto de la pantalla queda fijo.
    lv_obj_t* breathe = lv_label_create(scr);
    lv_label_set_text(breathe, t(K_CONSTRUYENDO_BOVEDA));
    lv_obj_set_style_text_color(breathe, C_TEXT, 0);
    lv_obj_set_style_text_font(breathe, FONT_MD, 0);
    lv_obj_set_style_text_align(breathe, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(breathe, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(breathe, UI_W - 48);
    lv_obj_align(breathe, LV_ALIGN_CENTER, 0, -46);
    lv_obj_set_style_opa(breathe, LV_OPA_COVER, 0);

    lv_obj_t* warn = lv_label_create(scr);
    lv_label_set_text(warn, t(K_NO_REINICIES));
    lv_obj_set_style_text_color(warn, C_MUTED, 0);
    lv_obj_set_style_text_font(warn, FONT_SM, 0);
    lv_obj_set_style_text_align(warn, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(warn, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(warn, UI_W - 48);
    lv_obj_align(warn, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t* track = lv_obj_create(scr);
    lv_obj_remove_style_all(track); // sin esto, el padding por defecto del tema casi no deja espacio visible en una barra de 10px
    lv_obj_set_size(track, UI_W - 80, 10);
    lv_obj_align(track, LV_ALIGN_CENTER, 0, 76);
    lv_obj_set_style_radius(track, 5, 0);
    lv_obj_set_style_bg_color(track, C_CARD, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* fill = lv_obj_create(track);
    lv_obj_remove_style_all(fill);
    lv_obj_set_size(fill, 0, 10);
    lv_obj_set_pos(fill, 0, 0);
    lv_obj_set_style_radius(fill, 5, 0);
    lv_obj_set_style_bg_color(fill, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    _ob_progress_fill = fill;

    lv_obj_t* pctlbl = lv_label_create(scr);
    lv_label_set_text(pctlbl, "0%");
    lv_obj_set_style_text_color(pctlbl, C_MUTED, 0);
    lv_obj_set_style_text_font(pctlbl, FONT_SM, 0);
    lv_obj_align(pctlbl, LV_ALIGN_CENTER, 0, 96);
    _ob_progress_pctlbl = pctlbl;

    _load_screen_instant(scr);

    vaultfs_init_begin();
    _ob_progress_timer = lv_timer_create(_ob_progress_timer_cb, 25, nullptr);
}

// ── Paso 9: Listo ────────────────────────────────────────────────
static void _show_ob_done() {
    lv_obj_t* scr = _make_screen();
    _ob_add_debug_skip(scr);

    lv_obj_t* icon = lv_label_create(scr);
    lv_label_set_text(icon, ICO_CHECK_CIRCLE);
    lv_obj_set_style_text_font(icon, FONT_ICON_EX, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x30D158), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -66);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, t(K_TODO_LISTO));
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_HDR_LG, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, UI_W - 40);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -16);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, t(K_BOVEDA_CONFIGURADA));
    lv_obj_set_style_text_color(sub, C_MUTED, 0);
    lv_obj_set_style_text_font(sub, FONT_SM, 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(sub, UI_W - 48);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 18);

    _ob_next_btn(scr, t(K_ACEPTAR), [](lv_event_t* e){
        vault_set_onboarding_done(true);
        _ob_active_flag = false;
        ui_main_show();
    });
    _load_screen_instant(scr);
}

// ── Paso 11: Aviso de privacidad ───────────────────────────
static void _show_ob_legal() {
    lv_obj_t* scr = _make_screen();
    _ob_add_back_btn(scr, 1); // volver a la bienvenida
    _ob_title(scr, t(K_LEGAL_TITULO), 12);

    // Texto scrolleable (mismo patrón que la explicación del Modo perdido).
    // El título ocupa 2 líneas (~60px) — la caja empieza debajo, y su alto
    // deja aire entre checkbox (y=212) y el botón Continuar (y=248).
    lv_obj_t* box = lv_obj_create(scr);
    lv_obj_set_size(box, UI_W - 32, 128);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 76);
    lv_obj_set_style_bg_color(box, C_CARD, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 12, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 12, 0);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);

    // Mismo texto completo que en Ajustes. LVGL copia y ajusta el contenido;
    // no se truncan los parrafos del aviso que configure el implementador.
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_t* notice = lv_label_create(box);
    lv_label_set_text(notice, t(K_LEGAL_TEXTO));
    lv_obj_set_style_text_color(notice, C_TEXT, 0);
    lv_obj_set_style_text_font(notice, FONT_SM, 0);
    lv_label_set_long_mode(notice, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(notice, UI_W - 32 - 24);

    lv_obj_t* cb = lv_checkbox_create(scr);
    lv_checkbox_set_text(cb, t(K_LEGAL_ACEPTO));
    lv_obj_set_style_text_color(cb, C_TEXT, 0);
    lv_obj_set_style_text_font(cb, FONT_SM, 0);
    lv_obj_set_style_bg_color(cb, C_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(cb, C_MUTED, LV_PART_INDICATOR);
    lv_obj_align(cb, LV_ALIGN_TOP_MID, 0, 212);

    lv_obj_t* btn = _ob_next_btn(scr, t(K_CONTINUAR), [](lv_event_t* e){
        _ob_legal_accepted = true;
        // Registrar lectura y version del aviso, no consentimiento legal.
        // Se conservan las claves NVS existentes por compatibilidad.
        Preferences p;
        if (p.begin("vprefs", false)) {
            p.putBool("legal_ok", true);
            p.putString("legal_ver", HP_PRIVACY_NOTICE_VERSION);
            p.end();
        }
        _ob_goto(2);
    });
    lv_obj_add_state(btn, LV_STATE_DISABLED); // hasta marcar la casilla
    lv_obj_add_event_cb(cb, [](lv_event_t* e){
        lv_obj_t* b = (lv_obj_t*)lv_event_get_user_data(e);
        if (lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED))
            lv_obj_clear_state(b, LV_STATE_DISABLED);
        else
            lv_obj_add_state(b, LV_STATE_DISABLED);
    }, LV_EVENT_VALUE_CHANGED, btn);

    _load_screen_instant(scr);
}

static void _ob_goto(int step) {
    _ob_step = step;
    switch (step) {
        case 0:  _show_ob_language(); break;
        case 1:  _show_ob_welcome(); break;
        case 2:  _show_ob_date(); break;
        case 3:  _show_ob_time(); break;
        case 4:  _show_ob_keyboard(); break;
        case 5:  _show_ob_locktime(); break;
        case 6:  _show_ob_theme(); break;
        case 7:  _show_ob_pin_create(); break;
        case 8:  _show_ob_pin_confirm(); break;
        case 9:  _show_ob_progress(); break;
        case 10: _show_ob_done(); break;
        case 11: _show_ob_legal(); break;
    }
}

// ── API Pública ───────────────────────────────────────────────
void onboarding_show() {
    _ob_active_flag = true;
    _current_screen = SCREEN_NONE; // evita que _sb_sync_visibility reactive la barra de estado
    _apply_theme(vault_get_theme());
    ui_statusbar_set_visible(false); // no debe verse mientras se configura el dispositivo
    _ob_goto(0);
}
