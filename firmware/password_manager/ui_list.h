#pragma once
// =============================================================
//  ui_list.h — pantalla principal (lista virtualizada + búsqueda
//  rápida, selección múltiple, menú FAB), detalle de entrada,
//  favoritos, categorías y búsqueda dedicada. Extraído de
//  ui_main.h. Depende de helpers/colores/fuentes/globales
//  definidos antes en ui_main.h (incluye _fab_open/_menu_* que
//  permanecen ahí porque también los usa ui_main_swipe_*).
// =============================================================
#include <lvgl.h>
#include "vault.h"
#include "i18n.h"

static void _show_totp_setup(const char* name);
static void _show_notes_full(const char* notes);
static void _show_discard_confirm(void (*on_discard)()); // definida en ui_forms.h
void kb_type_totp(const char* name); // definida en password_manager.ino

static void _close_menu_overlay(lv_obj_t* overlay) {
    if (!overlay) return;
    lv_obj_del(overlay);
    _menu_open        = false;
    _menu_panel_ref   = nullptr;
    _menu_overlay_ref = nullptr;
}

// Ordenar _entry_names según _sort_mode
// Item temporal para qsort — empareja nombre+timestamp para no perder
// sincronía entre ambos arreglos al reordenar.
struct _SortItem { char name[VAULT_FIELD_LEN]; uint32_t ts; };
static int _qsort_mode_g = 0; // qsort no permite pasar contexto al comparador

static int _sort_item_cmp(const void* a, const void* b) {
    const _SortItem* ia = (const _SortItem*)a;
    const _SortItem* ib = (const _SortItem*)b;
    switch (_qsort_mode_g) {
        case SORT_NAME_AZ:   return strcasecmp(ia->name, ib->name);
        case SORT_NAME_ZA:   return strcasecmp(ib->name, ia->name);
        case SORT_DATE_ASC:  return (ia->ts > ib->ts) - (ia->ts < ib->ts);
        case SORT_DATE_DESC: return (ib->ts > ia->ts) - (ib->ts < ia->ts);
    }
    return 0;
}

static void _sort_entries() {
    // Cargar timestamps para sort por fecha (PSRAM: 3500 enteros = 14KB,
    // preferible no acumular en stack en cada llamada de sort)
    static uint32_t* ts = nullptr;
    if (!ts) ts = (uint32_t*)ps_malloc(sizeof(uint32_t) * VAULT_MAX_ENTRIES);
    if (!ts) return;
    memset(ts, 0, sizeof(uint32_t) * VAULT_MAX_ENTRIES);
    if (_sort_mode == SORT_DATE_ASC || _sort_mode == SORT_DATE_DESC) {
        for (int i = 0; i < _entry_count; i++)
            ts[i] = vault_get_created_at(_entry_names[i]);
    }

    // qsort O(n log n) — con cientos/miles de entradas, el bubble sort
    // anterior (O(n²)) congelaba el dispositivo varios segundos cada vez
    // que se reordenaba la lista (ej. justo después de un import grande).
    static _SortItem* buf = nullptr;
    if (!buf) buf = (_SortItem*)ps_malloc(sizeof(_SortItem) * VAULT_MAX_ENTRIES);
    if (!buf) return;

    for (int i = 0; i < _entry_count; i++) {
        strncpy(buf[i].name, _entry_names[i], VAULT_FIELD_LEN - 1);
        buf[i].name[VAULT_FIELD_LEN - 1] = '\0';
        buf[i].ts = ts[i];
    }

    _qsort_mode_g = _sort_mode;
    qsort(buf, _entry_count, sizeof(_SortItem), _sort_item_cmp);

    for (int i = 0; i < _entry_count; i++) {
        strncpy(_entry_names[i], buf[i].name, VAULT_FIELD_LEN - 1);
        _entry_names[i][VAULT_FIELD_LEN - 1] = '\0';
        ts[i] = buf[i].ts;
    }
}

// Filtrar por búsqueda — devuelve índices que coinciden
static int* _filtered = nullptr; // PSRAM, asignado en ui_main_show()
static int _filtered_count = 0;
// Búsqueda case-insensitive sin strcasestr
static bool _str_contains_ci(const char* haystack, const char* needle) {
    if (!needle || strlen(needle) == 0) return true;
    int hlen = strlen(haystack);
    int nlen = strlen(needle);
    for (int i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (int j = 0; j < nlen; j++) {
            if (tolower((uint8_t)haystack[i+j]) != tolower((uint8_t)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static bool _str_starts_ci(const char* haystack, const char* needle) {
    if (!needle || strlen(needle) == 0) return true;
    int nlen = strlen(needle);
    if ((int)strlen(haystack) < nlen) return false;
    for (int j = 0; j < nlen; j++) {
        if (tolower((uint8_t)haystack[j]) != tolower((uint8_t)needle[j])) return false;
    }
    return true;
}

static void _apply_filter() {
    _filtered_count = 0;
    if (strlen(_search_buf) == 0) {
        for (int i = 0; i < _entry_count; i++)
            _filtered[_filtered_count++] = i;
        return;
    }
    // Primero: entradas que empiezan con el texto buscado
    for (int i = 0; i < _entry_count; i++) {
        if (_str_starts_ci(_entry_names[i], _search_buf))
            _filtered[_filtered_count++] = i;
    }
    // Segundo: entradas que contienen el texto pero no empiezan con él
    for (int i = 0; i < _entry_count; i++) {
        if (!_str_starts_ci(_entry_names[i], _search_buf) &&
             _str_contains_ci(_entry_names[i], _search_buf))
            _filtered[_filtered_count++] = i;
    }
}

// =============================================================
//  Virtualización de la lista — en vez de crear un widget LVGL
//  por cada entrada (lo cual satura la RAM interna con 500-1500
//  entradas y causa crashes de puntero nulo), se crea un pool FIJO
//  de widgets (suficiente para llenar la pantalla + margen) y se
//  reciclan: al hacer scroll, solo se les cambia el contenido y la
//  posición, nunca se crean/destruyen widgets nuevos por eso.
//  Un "spacer" invisible al final fuerza el alto de scroll correcto
//  para que la barra de desplazamiento se sienta real.
// =============================================================
#define VIRT_ROW_H   52                    // 46 alto fila + 6 separación
#define VIRT_CELL_W  ((UI_W - 8) / 3)      // 3 columnas en vista íconos
#define VIRT_CELL_H  72
#define VIRT_POOL    16                    // cubre pantalla + margen de sobra

static void _rebuild_list_content(); // forward decl — usada en callbacks definidos antes de su cuerpo

struct VirtRowW  { lv_obj_t* row=nullptr; lv_obj_t* name_lbl=nullptr; lv_obj_t* sub_lbl=nullptr; lv_obj_t* star=nullptr; lv_obj_t* arrow=nullptr; };
struct VirtCellW { lv_obj_t* cell=nullptr; lv_obj_t* circle=nullptr; lv_obj_t* ini_lbl=nullptr; lv_obj_t* name_lbl=nullptr; };
static VirtRowW  _vrow[VIRT_POOL];
static VirtCellW _vcell[VIRT_POOL];
static int _virt_pool_used = 0;   // cuántos slots del pool están activos
static int _virt_first_fi  = -1;  // primer índice filtrado visible (evita rebind si no cambió)
static int32_t _saved_scroll_y = 0; // posición de scroll a restaurar al volver del detalle
static bool _restore_scroll = false; // true = restaurar _saved_scroll_y en el próximo _show_list
static lv_obj_t* _fab_top_ref = nullptr; // botón flotante "volver arriba"

// Toast diferido para acciones sobre carpetas: como estas recargan la
// pantalla justo después (y la recarga taparía el toast), se guarda cuál
// mostrar y se dispara con lv_async_call tras estabilizar la pantalla.
// 0 = ninguno, 1 = creada (ok), 2 = ya existe (err), 3 = renombrada (ok)
static int _pending_toast = 0;
static int _batch_deleted_n = 0; // nº de entradas borradas en lote (para el toast diferido)
static void _flush_pending_cat_toast() {
    int p = _pending_toast;
    _pending_toast = 0;
    if (p == 0) return;
    lv_async_call([](void* arg){
        int which = (int)(intptr_t)arg;
        if      (which == 1) toast_ok(t(K_TOAST_CARPETA_CREADA));
        else if (which == 2) toast_err(t(K_TOAST_YA_EXISTE));
        else if (which == 3) toast_ok(t(K_TOAST_CARPETA_RENOMBRADA));
    }, (void*)(intptr_t)p);
}

// ── Vista lista: bind de contenido a un slot del pool ──────────
static void _virt_bind_row(int slot, int fi) {
    VirtRowW& w = _vrow[slot];
    int i = _filtered[fi];
    lv_obj_set_pos(w.row, 0, fi * VIRT_ROW_H);
    lv_obj_set_user_data(w.row, (void*)(intptr_t)fi);

    lv_label_set_text(w.name_lbl, _entry_names[i]);

    VaultListMetaItem* ve = _find_entry_meta(_entry_names[i]);
    if (ve && ve->type == VAULT_TYPE_WEB) lv_label_set_text(w.sub_lbl, ve->username);
    else lv_label_set_text(w.sub_lbl, t(K_OBJETO));

    bool selected = _sel_mode && _sel_map[fi];
    if (_sel_mode) {
        lv_obj_set_style_bg_color(w.row, selected ? C_ACCENT : C_CARD, 0);
        lv_obj_set_style_bg_opa(w.row, selected ? LV_OPA_30 : LV_OPA_COVER, 0);
        lv_obj_add_flag(w.star, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(w.arrow, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_bg_color(w.row, C_CARD, 0);
        lv_obj_set_style_bg_opa(w.row, LV_OPA_COVER, 0);
        lv_obj_clear_flag(w.arrow, LV_OBJ_FLAG_HIDDEN);
        if (ve && ve->favorite) lv_obj_clear_flag(w.star, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(w.star, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(w.row, LV_OBJ_FLAG_HIDDEN);
}

// Suprime el click fantasma que LVGL dispara al soltar tras un long press.
// Guarda el ÍNDICE presionado: solo se traga un click en ESA fila (el
// fantasma puede llegar tarde, tras el rebuild, sobre la fila reconstruida);
// un toque genuino en cualquier otra fila pasa de inmediato y desarma el guard.
static int _lp_guard_fi = -1;

static void _virt_row_click_cb(lv_event_t* e) {
    lv_obj_t* r = (lv_obj_t*)lv_event_get_target(e);
    int fi = (int)(intptr_t)lv_obj_get_user_data(r);
    if (_lp_guard_fi >= 0) {
        bool ghost = (fi == _lp_guard_fi);
        _lp_guard_fi = -1;
        if (ghost) return; // click fantasma del release sobre la fila long-presionada
    }
    if (fi < 0 || fi >= _filtered_count) return;
    if (_sel_mode) {
        _sel_map[fi] = !_sel_map[fi];
        _sel_count = 0;
        for (int k = 0; k < _filtered_count; k++) if (_sel_map[k]) _sel_count++;
        if (_sel_map[fi]) {
            lv_obj_set_style_bg_color(r, C_ACCENT, 0);
            lv_obj_set_style_bg_opa(r, LV_OPA_30, 0);
        } else {
            lv_obj_set_style_bg_color(r, C_CARD, 0);
            lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        }
        if (_sel_counter_lbl && lv_obj_is_valid(_sel_counter_lbl)) {
            char buf[32];
            snprintf(buf, sizeof(buf), t(K_SELECCIONADAS_FMT), _sel_count, _sel_count != 1 ? "s" : "");
            lv_label_set_text(_sel_counter_lbl, buf);
        }
        if (_sel_del_btn && lv_obj_is_valid(_sel_del_btn)) {
            if (_sel_count > 0) lv_obj_clear_state(_sel_del_btn, LV_STATE_DISABLED);
            else lv_obj_add_state(_sel_del_btn, LV_STATE_DISABLED);
        }
    } else {
        if (_list_cont && lv_obj_is_valid(_list_cont)) {
            _saved_scroll_y = lv_obj_get_scroll_y(_list_cont);
            _restore_scroll = true;
        }
        strncpy(_selected_name, _entry_names[_filtered[fi]], VAULT_FIELD_LEN-1);
        _show_detail(_selected_name);
    }
}

static void _virt_row_longpress_cb(lv_event_t* e) {
    if (_sel_mode) return;
    lv_obj_t* r = (lv_obj_t*)lv_event_get_target(e);
    int fi = (int)(intptr_t)lv_obj_get_user_data(r);
    if (fi < 0 || fi >= _filtered_count) return;
    _lp_guard_fi = fi;
    _sel_mode = true;
    memset(_sel_map, 0, sizeof(bool) * VAULT_MAX_ENTRIES);
    _sel_count = 1;
    _sel_map[fi] = true;
    lv_async_call([](void*){
        if (_search_cont && lv_obj_is_valid(_search_cont))
            lv_obj_add_flag(_search_cont, LV_OBJ_FLAG_HIDDEN);
        if (_btn_menu_ref && lv_obj_is_valid(_btn_menu_ref))
            lv_obj_add_flag(_btn_menu_ref, LV_OBJ_FLAG_HIDDEN);
        if (_sel_counter_lbl && lv_obj_is_valid(_sel_counter_lbl)) {
            lv_obj_clear_flag(_sel_counter_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(_sel_counter_lbl, t(K_1_SELECCIONADA));
        }
        if (_sel_all_btn && lv_obj_is_valid(_sel_all_btn)) {
            lv_obj_clear_flag(_sel_all_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_t* ico = lv_obj_get_child(_sel_all_btn, 0);
            if (ico) { lv_label_set_text(ico, ICO_SELECT_ALL); lv_obj_set_style_text_color(ico, C_ACCENT, 0); lv_obj_set_style_text_font(ico, FONT_ICON_EX, 0); }
        }
        if (_sel_del_btn && lv_obj_is_valid(_sel_del_btn)) {
            lv_obj_clear_flag(_sel_del_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_t* ico = lv_obj_get_child(_sel_del_btn, 0);
            if (ico) { lv_label_set_text(ico, ICO_DELETE); lv_obj_set_style_text_color(ico, C_DANGER, 0); }
        }
        if (_sel_cancel_btn && lv_obj_is_valid(_sel_cancel_btn))
            lv_obj_clear_flag(_sel_cancel_btn, LV_OBJ_FLAG_HIDDEN);
        if (_list_cont && lv_obj_is_valid(_list_cont)) {
            lv_obj_clean(_list_cont);
            _rebuild_list_content();
        }
    }, nullptr);
}

static void _virt_create_row(lv_obj_t* list, int slot) {
    VirtRowW& w = _vrow[slot];
    w.row = lv_obj_create(list);
    lv_obj_set_size(w.row, UI_W - 16, 46);
    lv_obj_set_style_border_width(w.row, 0, 0);
    lv_obj_set_style_pad_all(w.row, 0, 0);
    lv_obj_set_style_radius(w.row, 12, 0);
    lv_obj_clear_flag(w.row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(w.row, C_CARD, 0);
    lv_obj_set_style_bg_opa(w.row, LV_OPA_COVER, 0);
    _press_fb(w.row);

    w.name_lbl = lv_label_create(w.row);
    lv_obj_set_style_text_color(w.name_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(w.name_lbl, FONT_MD, 0);
    lv_label_set_long_mode(w.name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(w.name_lbl, UI_W - 64);
    lv_obj_set_pos(w.name_lbl, 12, 6);
    lv_obj_clear_flag(w.name_lbl, LV_OBJ_FLAG_CLICKABLE);

    w.sub_lbl = lv_label_create(w.row);
    lv_obj_set_style_text_color(w.sub_lbl, C_MUTED, 0);
    lv_obj_set_style_text_font(w.sub_lbl, FONT_SM, 0);
    lv_label_set_long_mode(w.sub_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(w.sub_lbl, UI_W - 64);
    lv_obj_set_pos(w.sub_lbl, 12, 26);
    lv_obj_clear_flag(w.sub_lbl, LV_OBJ_FLAG_CLICKABLE);

    w.star = lv_label_create(w.row);
    lv_label_set_text(w.star, ICO_STAR);
    lv_obj_set_style_text_font(w.star, FONT_ICON, 0);
    lv_obj_set_style_text_color(w.star, lv_color_hex(0xFFCC00), 0);
    lv_obj_set_pos(w.star, UI_W - 52, 14);
    lv_obj_clear_flag(w.star, LV_OBJ_FLAG_CLICKABLE);

    w.arrow = lv_label_create(w.row);
    lv_label_set_text(w.arrow, ICO_CHEVRON_RIGHT);
    lv_obj_set_style_text_font(w.arrow, FONT_ICON, 0);
    lv_obj_set_style_text_color(w.arrow, C_MUTED, 0);
    lv_obj_set_pos(w.arrow, UI_W - 36, 14);
    lv_obj_clear_flag(w.arrow, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(w.row, _virt_row_click_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(w.row, _virt_row_longpress_cb, LV_EVENT_LONG_PRESSED, nullptr);
}

// ── Vista íconos: bind de contenido a un slot del pool ─────────
static void _virt_bind_cell(int slot, int fi) {
    VirtCellW& w = _vcell[slot];
    int i = _filtered[fi];
    int col = fi % 3, row = fi / 3;
    lv_obj_set_pos(w.cell, col * VIRT_CELL_W, row * VIRT_CELL_H);
    lv_obj_set_user_data(w.cell, (void*)(intptr_t)fi);

    bool selected = _sel_mode && _sel_map[fi];
    lv_obj_set_style_bg_color(w.circle, selected ? C_ACCENT : C_BORDER, 0);

    char ini[2] = {(char)toupper(_entry_names[i][0]), '\0'};
    lv_label_set_text(w.ini_lbl, ini);
    lv_label_set_text(w.name_lbl, _entry_names[i]);
    lv_obj_clear_flag(w.cell, LV_OBJ_FLAG_HIDDEN);
}

static void _virt_cell_click_cb(lv_event_t* e) {
    lv_obj_t* c = (lv_obj_t*)lv_event_get_target(e);
    int fi = (int)(intptr_t)lv_obj_get_user_data(c);
    if (_lp_guard_fi >= 0) {
        bool ghost = (fi == _lp_guard_fi);
        _lp_guard_fi = -1;
        if (ghost) return; // click fantasma del release sobre la celda long-presionada
    }
    if (fi < 0 || fi >= _filtered_count) return;
    if (_sel_mode) {
        _sel_map[fi] = !_sel_map[fi];
        _sel_count = 0;
        for (int k = 0; k < _filtered_count; k++) if (_sel_map[k]) _sel_count++;
        lv_obj_t* circ = lv_obj_get_child(c, 0);
        if (circ) lv_obj_set_style_bg_color(circ, _sel_map[fi] ? C_ACCENT : C_BORDER, 0);
        if (_sel_counter_lbl && lv_obj_is_valid(_sel_counter_lbl)) {
            char buf[32];
            snprintf(buf, sizeof(buf), t(K_SELECCIONADAS_FMT), _sel_count, _sel_count != 1 ? "s" : "");
            lv_label_set_text(_sel_counter_lbl, buf);
        }
    } else {
        if (_list_cont && lv_obj_is_valid(_list_cont)) {
            _saved_scroll_y = lv_obj_get_scroll_y(_list_cont);
            _restore_scroll = true;
        }
        strncpy(_selected_name, _entry_names[_filtered[fi]], VAULT_FIELD_LEN-1);
        _show_detail(_selected_name);
    }
}

static void _virt_cell_longpress_cb(lv_event_t* e) {
    if (_sel_mode) return;
    lv_obj_t* c = (lv_obj_t*)lv_event_get_target(e);
    int fi = (int)(intptr_t)lv_obj_get_user_data(c);
    if (fi < 0 || fi >= _filtered_count) return;
    _lp_guard_fi = fi;
    _sel_mode = true;
    memset(_sel_map, 0, sizeof(bool) * VAULT_MAX_ENTRIES);
    _sel_count = 1;
    _sel_map[fi] = true;
    lv_async_call([](void*){
        if (_search_cont && lv_obj_is_valid(_search_cont))
            lv_obj_add_flag(_search_cont, LV_OBJ_FLAG_HIDDEN);
        if (_btn_menu_ref && lv_obj_is_valid(_btn_menu_ref))
            lv_obj_add_flag(_btn_menu_ref, LV_OBJ_FLAG_HIDDEN);
        if (_sel_counter_lbl && lv_obj_is_valid(_sel_counter_lbl)) {
            lv_obj_clear_flag(_sel_counter_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(_sel_counter_lbl, t(K_1_SELECCIONADA));
        }
        if (_sel_all_btn && lv_obj_is_valid(_sel_all_btn)) {
            lv_obj_t* ico = lv_obj_get_child(_sel_all_btn, 0);
            if (ico) { lv_label_set_text(ico, ICO_SELECT_ALL); lv_obj_set_style_text_color(ico, C_ACCENT, 0); lv_obj_set_style_text_font(ico, FONT_ICON_EX, 0); }
        }
        if (_sel_del_btn && lv_obj_is_valid(_sel_del_btn)) {
            lv_obj_t* ico = lv_obj_get_child(_sel_del_btn, 0);
            if (ico) { lv_label_set_text(ico, ICO_DELETE); lv_obj_set_style_text_color(ico, C_DANGER, 0); }
        }
        if (_sel_cancel_btn && lv_obj_is_valid(_sel_cancel_btn))
            lv_obj_clear_flag(_sel_cancel_btn, LV_OBJ_FLAG_HIDDEN);
        if (_list_cont && lv_obj_is_valid(_list_cont)) {
            lv_obj_clean(_list_cont);
            _rebuild_list_content();
        }
    }, nullptr);
}

static void _virt_create_cell(lv_obj_t* list, int slot) {
    VirtCellW& w = _vcell[slot];
    w.cell = lv_obj_create(list);
    lv_obj_set_size(w.cell, VIRT_CELL_W, VIRT_CELL_H);
    lv_obj_set_style_border_width(w.cell, 0, 0);
    lv_obj_set_style_pad_all(w.cell, 0, 0);
    lv_obj_set_style_radius(w.cell, 0, 0);
    lv_obj_set_style_bg_opa(w.cell, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(w.cell, LV_OBJ_FLAG_SCROLLABLE);
    _press_fb(w.cell);

    int ico_size = 38;
    w.circle = lv_obj_create(w.cell);
    lv_obj_set_size(w.circle, ico_size, ico_size);
    lv_obj_set_pos(w.circle, (VIRT_CELL_W - ico_size) / 2, 0);
    lv_obj_set_style_radius(w.circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(w.circle, C_BORDER, 0);
    lv_obj_set_style_bg_opa(w.circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(w.circle, 0, 0);
    lv_obj_set_style_pad_all(w.circle, 0, 0);
    lv_obj_clear_flag(w.circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(w.circle, LV_OBJ_FLAG_CLICKABLE);

    w.ini_lbl = lv_label_create(w.circle);
    lv_obj_set_style_text_color(w.ini_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(w.ini_lbl, FONT_MD, 0);
    lv_obj_center(w.ini_lbl);
    lv_obj_clear_flag(w.ini_lbl, LV_OBJ_FLAG_CLICKABLE);

    w.name_lbl = lv_label_create(w.cell);
    lv_obj_set_style_text_color(w.name_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(w.name_lbl, FONT_SM, 0);
    lv_obj_set_style_text_align(w.name_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(w.name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_size(w.name_lbl, VIRT_CELL_W, 18);
    lv_obj_set_pos(w.name_lbl, 0, ico_size + 4);
    lv_obj_clear_flag(w.name_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(w.cell, _virt_cell_click_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(w.cell, _virt_cell_longpress_cb, LV_EVENT_LONG_PRESSED, nullptr);
}

// Spacer invisible al final del contenido — fuerza el alto real de
// scroll (en píxeles) sin necesidad de tener un widget por entrada.
static void _virt_make_spacer(lv_obj_t* list, int total_h) {
    lv_obj_t* spacer = lv_obj_create(list);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_pos(spacer, 0, total_h);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
}

// Al hacer scroll: recalcula qué rango de índices filtrados debería
// verse y solo re-vincula los slots del pool si cambió el primero
// visible (evita trabajo innecesario en cada pixel de scroll).
static void _virt_scroll_cb(lv_event_t* e) {
    if (!_list_cont || !lv_obj_is_valid(_list_cont)) return;
    int32_t scroll_y = lv_obj_get_scroll_y(_list_cont);
    if (scroll_y < 0) scroll_y = 0;

    if (_view_icons) {
        int first_row = scroll_y / VIRT_CELL_H;
        int first_fi  = first_row * 3;
        if (first_fi == _virt_first_fi) return;
        _virt_first_fi = first_fi;
        for (int slot = 0; slot < _virt_pool_used; slot++) {
            int fi = first_fi + slot;
            if (fi < _filtered_count) _virt_bind_cell(slot, fi);
            else lv_obj_add_flag(_vcell[slot].cell, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        int first_fi = scroll_y / VIRT_ROW_H;
        if (first_fi == _virt_first_fi) return;
        _virt_first_fi = first_fi;
        for (int slot = 0; slot < _virt_pool_used; slot++) {
            int fi = first_fi + slot;
            if (fi < _filtered_count) _virt_bind_row(slot, fi);
            else lv_obj_add_flag(_vrow[slot].row, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Re-vincula los slots visibles del pool a la posición de scroll actual.
// Se llama tras restaurar el scroll manualmente (sin evento LVGL de por medio).
static void _virt_scroll_cb_manual() {
    if (!_list_cont || !lv_obj_is_valid(_list_cont)) return;
    if (_virt_pool_used <= 0) return; // pool aún no creado
    int32_t scroll_y = lv_obj_get_scroll_y(_list_cont);
    if (scroll_y < 0) scroll_y = 0;
    if (_view_icons) {
        int first_row = scroll_y / VIRT_CELL_H;
        int first_fi  = first_row * 3;
        _virt_first_fi = first_fi;
        for (int slot = 0; slot < _virt_pool_used; slot++) {
            if (!_vcell[slot].cell || !lv_obj_is_valid(_vcell[slot].cell)) continue;
            int fi = first_fi + slot;
            if (fi < _filtered_count) _virt_bind_cell(slot, fi);
            else lv_obj_add_flag(_vcell[slot].cell, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        int first_fi = scroll_y / VIRT_ROW_H;
        _virt_first_fi = first_fi;
        for (int slot = 0; slot < _virt_pool_used; slot++) {
            if (!_vrow[slot].row || !lv_obj_is_valid(_vrow[slot].row)) continue;
            int fi = first_fi + slot;
            if (fi < _filtered_count) _virt_bind_row(slot, fi);
            else lv_obj_add_flag(_vrow[slot].row, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void _rebuild_list_content() {
    if (!_list_cont || !lv_obj_is_valid(_list_cont)) return;
    lv_obj_t* list = _list_cont;

    lv_obj_set_style_pad_all(list, 8, 0);
    lv_obj_set_style_pad_row(list, 0, 0);
    lv_obj_set_style_pad_column(list, 0, 0);
    lv_obj_set_layout(list, 0); // 0 = sin layout (quita el flex previo de la vista íconos) para posicionamiento manual
    lv_obj_remove_event_cb(list, _virt_scroll_cb); // evitar doble registro entre rebuilds

    _virt_first_fi  = -1;
    _virt_pool_used = 0;

    if (_filtered_count == 0) {
        lv_obj_t* empty = lv_label_create(list);
        lv_label_set_text(empty,
            strlen(_search_buf) > 0 ? t(K_SIN_RESULTADOS) : t(K_SIN_ENTRADAS_TOCA));
        lv_obj_set_style_text_color(empty, C_MUTED, 0);
        lv_obj_set_style_text_font(empty, FONT_SM, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(empty, LV_ALIGN_CENTER, 0, -20);
        return;
    }

    int pool_n = (VIRT_POOL < _filtered_count) ? VIRT_POOL : _filtered_count;
    _virt_pool_used = pool_n;

    if (_view_icons) {
        lv_obj_set_style_pad_all(list, 4, 0);
        for (int slot = 0; slot < pool_n; slot++) _virt_create_cell(list, slot);
        for (int slot = 0; slot < pool_n; slot++) _virt_bind_cell(slot, slot);
        int rows_total = (_filtered_count + 2) / 3;
        _virt_make_spacer(list, rows_total * VIRT_CELL_H);
    } else {
        for (int slot = 0; slot < pool_n; slot++) _virt_create_row(list, slot);
        for (int slot = 0; slot < pool_n; slot++) _virt_bind_row(slot, slot);
        _virt_make_spacer(list, _filtered_count * VIRT_ROW_H);
    }
    _virt_first_fi = 0;

    lv_obj_add_event_cb(list, _virt_scroll_cb, LV_EVENT_SCROLL, nullptr);
}

static void _show_list(bool back=true) {
    _current_screen = SCREEN_LIST; _kb_active = false;
    if (!_entry_names) {
        _entry_names = (char(*)[VAULT_FIELD_LEN]) ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES);
        if (!_entry_names) { Serial.println("[UI] FATAL: sin PSRAM"); return; }
    }
    if (!_filtered) {
        _filtered = (int*) ps_malloc(sizeof(int) * VAULT_MAX_ENTRIES);
        if (!_filtered) { Serial.println("[UI] FATAL: sin PSRAM para _filtered"); return; }
    }
    if (!_sel_map) {
        _sel_map = (bool*) ps_malloc(sizeof(bool) * VAULT_MAX_ENTRIES);
        if (_sel_map) memset(_sel_map, 0, sizeof(bool) * VAULT_MAX_ENTRIES);
        else Serial.println("[UI] FATAL: sin PSRAM para _sel_map");
    }
    if (!_entry_meta) {
        _entry_meta = (VaultListMetaItem*) ps_malloc(sizeof(VaultListMetaItem) * VAULT_MAX_ENTRIES);
        if (!_entry_meta) Serial.println("[UI] sin PSRAM para metadata de lista");
    }
    // Cargar preferencias de vista desde NVS (solo una vez)
    if (!_prefs_loaded) {
        _sort_mode   = vault_get_sort();
        _view_icons  = vault_get_view_icons();
        _prefs_loaded = true;
    }
    vault_list(_entry_names, &_entry_count, VAULT_MAX_ENTRIES);
    _entry_meta_count = _entry_meta ? vault_list_meta(_entry_meta, VAULT_MAX_ENTRIES) : 0;
    _sort_entries();
    _apply_filter();
    _fab_open  = false;
    _menu_open = false;
    // No resetear _sel_mode aquí — se resetea explícitamente con botón X o después de eliminar

    lv_obj_t* scr = _make_screen();

    // Contenedor desplazado para dejarle espacio a la barra de estado
    // (batería+hora, vive en lv_layer_top(), independiente de esta
    // pantalla). La 'bar' inferior (botón Agregar) NO se reparenta
    // aquí — ya está anclada al fondo y no necesita el desplazamiento.
    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, UI_W, UI_H - STATUSBAR_H);
    lv_obj_set_pos(body, 0, STATUSBAR_H);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // ── Header ────────────────────────────────────────────────
    _sel_counter_lbl = nullptr;
    _sel_del_btn     = nullptr;
    _sel_all_btn     = nullptr;
    _sel_cancel_btn  = nullptr;
    _search_cont     = nullptr;

    // Botón hamburguesa — abre el menú (reemplaza el gesto de swipe)
    lv_obj_t* btn_menu = lv_btn_create(body);
    _btn_menu_ref = btn_menu;
    _press_fb(btn_menu);
    lv_obj_set_size(btn_menu, 44, 36);
    lv_obj_set_pos(btn_menu, 0, 0);
    lv_obj_set_style_bg_opa(btn_menu, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_menu, 0, 0);
    lv_obj_set_style_border_width(btn_menu, 0, 0);
    lv_obj_t* menu_ico = lv_label_create(btn_menu);
    lv_label_set_text(menu_ico, ICO_MENU);
    lv_obj_set_style_text_color(menu_ico, C_TEXT, 0);
    lv_obj_set_style_text_font(menu_ico, FONT_ICON, 0);
    lv_obj_center(menu_ico);
    if (_sel_mode) lv_obj_add_flag(btn_menu, LV_OBJ_FLAG_HIDDEN);

    // Buscador — contenedor para poder ocultarlo
    lv_obj_t* search_cont = lv_obj_create(body);
    _search_cont = search_cont;
    lv_obj_set_size(search_cont, UI_W - 88 - 44, 36);
    lv_obj_set_pos(search_cont, 44, 0);
    lv_obj_set_style_bg_opa(search_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(search_cont, 0, 0);
    lv_obj_set_style_pad_all(search_cont, 0, 0);
    lv_obj_clear_flag(search_cont, LV_OBJ_FLAG_SCROLLABLE);
    if (_sel_mode) lv_obj_add_flag(search_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* search_ta = lv_textarea_create(search_cont);
    lv_obj_set_width(search_ta, UI_W - 88 - 44 - 8);
    lv_obj_align(search_ta, LV_ALIGN_LEFT_MID, 8, 0); // centrado en el header;
    // la altura real la fijan min/max_height de abajo (28px)
    lv_textarea_set_one_line(search_ta, true);
    lv_textarea_set_placeholder_text(search_ta, t(K_BUSCAR_PLACEHOLDER));
    if (strlen(_search_buf) > 0) lv_textarea_set_text(search_ta, _search_buf);
    lv_obj_set_style_bg_color(search_ta, C_CARD, 0);
    lv_obj_set_style_bg_opa(search_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(search_ta, 0, 0);
    lv_obj_set_style_radius(search_ta, 8, 0);
    lv_obj_set_style_text_color(search_ta, C_TEXT, 0);
    lv_obj_set_style_text_color(search_ta, C_MUTED, LV_PART_TEXTAREA_PLACEHOLDER); // si no, usa el gris por defecto de LVGL, no el nuestro
    lv_obj_set_style_text_font(search_ta, FONT_SM, 0);
    lv_obj_set_style_pad_hor(search_ta, 10, 0);
    lv_obj_set_style_pad_top(search_ta, 6, 0);
    lv_obj_set_style_pad_bottom(search_ta, 6, 0);
    lv_obj_set_style_min_height(search_ta, 28, 0);
    lv_obj_set_style_max_height(search_ta, 28, 0);
    lv_obj_set_scrollbar_mode(search_ta, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(search_ta, [](lv_event_t* e){ _show_search(); }, LV_EVENT_CLICKED, nullptr);

    // Botón X — salir de modo selección (oculto por defecto)
    lv_obj_t* btn_cancel = lv_btn_create(body);
    _press_fb(btn_cancel);
    _sel_cancel_btn = btn_cancel;
    lv_obj_set_size(btn_cancel, 44, 36);
    lv_obj_set_pos(btn_cancel, 0, 0);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_cancel, 0, 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_t* cancel_ico = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_ico, ICO_CLOSE);
    lv_obj_set_style_text_color(cancel_ico, C_TEXT, 0);
    lv_obj_set_style_text_font(cancel_ico, FONT_ICON, 0);
    lv_obj_center(cancel_ico);
    if (!_sel_mode) lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* ev){
        _sel_mode = false;
        _sel_count = 0;
        memset(_sel_map, 0, sizeof(bool) * VAULT_MAX_ENTRIES);
        lv_async_call([](void*){ _show_list(false); }, nullptr);
    }, LV_EVENT_CLICKED, nullptr);

    // Contador de selección (oculto por defecto)
    lv_obj_t* sel_lbl = lv_label_create(body);
    _sel_counter_lbl = sel_lbl;
    char sel_buf[32];
    snprintf(sel_buf, sizeof(sel_buf), t(K_SELECCIONADAS_FMT), _sel_count, _sel_count != 1 ? "s" : "");
    lv_label_set_text(sel_lbl, sel_buf);
    lv_obj_set_style_text_color(sel_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(sel_lbl, FONT_MD, 0);
    lv_obj_set_pos(sel_lbl, 48, 10);
    if (!_sel_mode) lv_obj_add_flag(sel_lbl, LV_OBJ_FLAG_HIDDEN);

    // Botón vista / seleccionar todo
    lv_obj_t* btn_view = lv_btn_create(body);
    _press_fb(btn_view);
    _sel_all_btn = btn_view;
    lv_obj_set_size(btn_view, 40, 36);
    lv_obj_set_pos(btn_view, UI_W - 82, 0);
    lv_obj_set_style_bg_opa(btn_view, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_view, 0, 0);
    lv_obj_set_style_border_width(btn_view, 0, 0);
    lv_obj_t* view_ico = lv_label_create(btn_view);
    lv_label_set_text(view_ico, _sel_mode ? ICO_SELECT_ALL : (_view_icons ? ICO_APPS : ICO_LIST));
    lv_obj_set_style_text_color(view_ico, _sel_mode ? C_ACCENT : C_TEXT, 0);
    lv_obj_set_style_text_font(view_ico, FONT_ICON_EX, 0);
    lv_obj_center(view_ico);
    lv_obj_add_event_cb(btn_view, [](lv_event_t* ev){
        if (_sel_mode) {
            // Seleccionar / deseleccionar todo
            bool all = (_sel_count == _filtered_count);
            _sel_count = 0;
            for (int k = 0; k < _filtered_count; k++) {
                _sel_map[k] = !all;
                if (!all) _sel_count++;
            }
            if (_sel_counter_lbl && lv_obj_is_valid(_sel_counter_lbl)) {
                char buf[32];
                snprintf(buf, sizeof(buf), t(K_SELECCIONADAS_FMT), _sel_count, _sel_count != 1 ? "s" : "");
                lv_label_set_text(_sel_counter_lbl, buf);
            }
            if (_list_cont && lv_obj_is_valid(_list_cont)) {
                _restore_scroll = false; // el scroll guardado no aplica tras filtrar
                _virt_pool_used = 0;
                _virt_first_fi  = -1;
                lv_obj_remove_event_cb(_list_cont, _virt_scroll_cb);
                lv_obj_clean(_list_cont);
                _rebuild_list_content();
            }
        } else {
            _view_icons = !_view_icons;
            vault_set_view_icons(_view_icons);
            lv_obj_t* ico = lv_obj_get_child((lv_obj_t*)lv_event_get_target(ev), 0);
            lv_label_set_text(ico, _view_icons ? ICO_APPS : ICO_LIST);
            if (_list_cont && lv_obj_is_valid(_list_cont)) {
                // Al cambiar de vista, el pool actual se destruye. Resetear el
                // estado del pool ANTES del rebuild asíncrono, para que ningún
                // evento de scroll intermedio toque objetos ya liberados. El
                // scroll guardado tampoco aplica entre geometrías distintas.
                _restore_scroll = false;
                _virt_pool_used = 0;
                _virt_first_fi  = -1;
                lv_obj_remove_event_cb(_list_cont, _virt_scroll_cb);
                lv_obj_clean(_list_cont);
                lv_async_call([](void*){ _rebuild_list_content(); }, nullptr);
            }
        }
    }, LV_EVENT_CLICKED, nullptr);

    // Botón sort / eliminar
    lv_obj_t* btn_sort = lv_btn_create(body);
    _press_fb(btn_sort);
    _sel_del_btn = btn_sort;
    lv_obj_set_size(btn_sort, 40, 36);
    lv_obj_set_pos(btn_sort, UI_W - 40, 0);
    lv_obj_set_style_bg_opa(btn_sort, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_sort, 0, 0);
    lv_obj_set_style_border_width(btn_sort, 0, 0);
    lv_obj_t* sort_ico = lv_label_create(btn_sort);
    lv_label_set_text(sort_ico, _sel_mode ? ICO_DELETE : ICO_SORT);
    lv_obj_set_style_text_color(sort_ico, _sel_mode ? C_DANGER : C_TEXT, 0);
    lv_obj_set_style_text_font(sort_ico, FONT_ICON, 0);
    // Deshabilitar sort en modo selección
    if (!_sel_mode) {
        lv_obj_set_style_opa(btn_sort, LV_OPA_COVER, 0);
    }
    lv_obj_center(sort_ico);

    _divider(body, 36);

    // ── Lista / Iconos ────────────────────────────────────────
    // Header 36px + barra inferior 52px + pad superior 4px: entran 4
    // filas completas (necesitan 206px) con 8px de holgura (list_h=214).
    int list_y = 36;
    int list_h = UI_H - list_y - 52 - STATUSBAR_H;

    lv_obj_t* list = lv_obj_create(body);
    _list_cont = list;
    lv_obj_set_size(list, UI_W, list_h);
    lv_obj_set_pos(list, 0, list_y);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_hor(list, 8, 0);
    lv_obj_set_style_pad_top(list, 4, 0);
    lv_obj_set_style_pad_bottom(list, 8, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    _rebuild_list_content();

    // Restaurar la posición de scroll si venimos de ver el detalle de una
    // entrada (para no volver siempre al principio de una lista larga).
    if (_restore_scroll && _saved_scroll_y > 0) {
        lv_obj_scroll_to_y(list, _saved_scroll_y, LV_ANIM_OFF);
        _virt_scroll_cb_manual(); // re-vincular los slots visibles a la nueva posición
    }
    _restore_scroll = false;

    // ── Botón flotante "volver arriba" ────────────────────────
    // Aparece al desplazarse hacia abajo en listas largas; al tocarlo
    // salta al inicio de la lista. Oculto cuando ya estás arriba.
    lv_obj_t* fab_top = lv_btn_create(body);
    _fab_top_ref = fab_top;
    _press_fb(fab_top);
    lv_obj_set_size(fab_top, 40, 40);
    lv_obj_set_pos(fab_top, UI_W - 52, list_y + list_h - 52);
    lv_obj_set_style_radius(fab_top, 20, 0);
    lv_obj_set_style_bg_color(fab_top, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(fab_top, LV_OPA_80, 0);
    lv_obj_set_style_shadow_width(fab_top, 8, 0);
    lv_obj_set_style_shadow_opa(fab_top, LV_OPA_30, 0);
    lv_obj_set_style_border_width(fab_top, 0, 0);
    lv_obj_add_flag(fab_top, LV_OBJ_FLAG_HIDDEN); // oculto hasta que se scrollee
    lv_obj_t* fab_top_ico = lv_label_create(fab_top);
    lv_label_set_text(fab_top_ico, LV_SYMBOL_UP);
    lv_obj_set_style_text_color(fab_top_ico, lv_color_white(), 0);
    lv_obj_center(fab_top_ico);
    lv_obj_add_event_cb(fab_top, [](lv_event_t* e){
        if (_list_cont && lv_obj_is_valid(_list_cont)) {
            lv_obj_scroll_to_y(_list_cont, 0, LV_ANIM_ON);
        }
    }, LV_EVENT_CLICKED, nullptr);
    // Mostrar/ocultar el botón según la posición de scroll
    lv_obj_add_event_cb(list, [](lv_event_t* e){
        if (!_fab_top_ref || !lv_obj_is_valid(_fab_top_ref)) return;
        if (!_list_cont || !lv_obj_is_valid(_list_cont)) return;
        int32_t y = lv_obj_get_scroll_y(_list_cont);
        if (y > 120) lv_obj_clear_flag(_fab_top_ref, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(_fab_top_ref, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_SCROLL, nullptr);
    // Si restauramos scroll, evaluar visibilidad inicial del botón
    if (_saved_scroll_y > 120 && _fab_top_ref) lv_obj_clear_flag(_fab_top_ref, LV_OBJ_FLAG_HIDDEN);

    // ── Barra inferior — botón Agregar ───────────────────────
    lv_obj_t* bar = lv_obj_create(scr);
    lv_obj_set_size(bar, UI_W, 52);
    lv_obj_set_pos(bar, 0, UI_H - 52);
    lv_obj_set_style_bg_color(bar, C_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bar, C_BORDER, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* fab = lv_btn_create(bar);
    _press_fb(fab);
    lv_obj_set_size(fab, UI_W - 32, 40);
    lv_obj_align(fab, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(fab, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(fab, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fab, 0, 0);
    lv_obj_set_style_radius(fab, 12, 0);
    lv_obj_set_style_shadow_width(fab, 0, 0);

    lv_obj_t* fab_row = lv_obj_create(fab);
    lv_obj_set_size(fab_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(fab_row, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(fab_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fab_row, 0, 0);
    lv_obj_set_style_pad_all(fab_row, 0, 0);
    lv_obj_set_flex_flow(fab_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fab_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(fab_row, 6, 0);
    lv_obj_clear_flag(fab_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(fab_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* fab_ico = lv_label_create(fab_row);
    lv_label_set_text(fab_ico, ICO_ADD); lv_obj_set_style_text_font(fab_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(fab_ico, C_ACCENT_TEXT, 0);
    lv_obj_set_style_text_font(fab_ico, FONT_ICON, 0);
    lv_obj_clear_flag(fab_ico, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* fab_lbl = lv_label_create(fab_row);
    lv_label_set_text(fab_lbl, t(K_AGREGAR));
    lv_obj_set_style_text_color(fab_lbl, C_ACCENT_TEXT, 0);
    lv_obj_set_style_text_font(fab_lbl, FONT_MD, 0);
    lv_obj_clear_flag(fab_lbl, LV_OBJ_FLAG_CLICKABLE);

    // ── Bottom sheet: Sort/Vista ──────────────────────────────
    lv_obj_add_event_cb(btn_sort, [](lv_event_t* e){
        // Modo selección — botón eliminar
        if (_sel_mode) {
            if (_sel_count == 0) return;
            // Dialog de confirmación
            lv_obj_t* scr_act = lv_scr_act();
            lv_obj_t* overlay = lv_obj_create(scr_act);
            lv_obj_set_size(overlay, UI_W, UI_H);
            lv_obj_set_pos(overlay, 0, 0);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
            _fade_overlay(overlay, LV_OPA_50);
            lv_obj_set_style_border_width(overlay, 0, 0);
            lv_obj_set_style_pad_all(overlay, 0, 0);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* dlg = lv_obj_create(overlay);
            lv_obj_set_size(dlg, UI_W - 48, 140);
            lv_obj_align(dlg, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_bg_color(dlg, C_CARD, 0); lv_obj_set_style_bg_opa(dlg, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(dlg, 0, 0); lv_obj_set_style_radius(dlg, 12, 0);
            lv_obj_set_style_pad_all(dlg, 16, 0);
            lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(dlg, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* msg = lv_label_create(dlg);
            char buf[112];
            if (_sel_count == 1) snprintf(buf, sizeof(buf), "%s", t(K_CONFIRMA_ELIMINAR_ENTRADA));
            else snprintf(buf, sizeof(buf), t(K_CONFIRMA_ELIMINAR_LOTE_FMT), _sel_count);
            lv_label_set_text(msg, buf);
            lv_obj_set_style_text_color(msg, C_TEXT, 0);
            lv_obj_set_style_text_font(msg, FONT_MD, 0);
            lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 0);

            int bw_s = (UI_W - 48 - 32 - 8) / 2;
            // Cancelar — izquierda
            lv_obj_t* btn_no = lv_btn_create(dlg);
            _press_fb(btn_no);
            lv_obj_set_size(btn_no, bw_s, 36); lv_obj_set_pos(btn_no, 0, 72);
            lv_obj_set_style_bg_color(btn_no, C_BORDER, 0); lv_obj_set_style_radius(btn_no, 8, 0);
            lv_obj_set_style_shadow_width(btn_no, 0, 0); lv_obj_set_style_border_width(btn_no, 1, 0);
            lv_obj_set_style_border_color(btn_no, C_MUTED, 0);
            lv_obj_t* lbl_no = lv_label_create(btn_no); lv_label_set_text(lbl_no, t(K_CANCELAR));
            lv_obj_set_style_text_font(lbl_no, FONT_SM, 0);
            lv_obj_set_style_text_color(lbl_no, C_TEXT, 0); lv_obj_center(lbl_no);
            lv_obj_add_event_cb(btn_no, [](lv_event_t* ev){
                lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
            }, LV_EVENT_CLICKED, nullptr);
            // Eliminar — derecha
            lv_obj_t* btn_yes = lv_btn_create(dlg);
            _press_fb(btn_yes);
            lv_obj_set_size(btn_yes, bw_s, 36); lv_obj_set_pos(btn_yes, bw_s + 8, 72);
            lv_obj_set_style_bg_color(btn_yes, C_DANGER, 0); lv_obj_set_style_radius(btn_yes, 8, 0);
            lv_obj_set_style_shadow_width(btn_yes, 0, 0); lv_obj_set_style_border_width(btn_yes, 0, 0);
            lv_obj_t* lbl_yes = lv_label_create(btn_yes); lv_label_set_text(lbl_yes, t(K_ELIMINAR));
            lv_obj_set_style_text_font(lbl_yes, FONT_SM, 0);
            lv_obj_set_style_text_color(lbl_yes, lv_color_hex(0xFFFFFF), 0); lv_obj_center(lbl_yes);
            lv_obj_add_event_cb(btn_yes, [](lv_event_t* ev){
                // Eliminar entradas seleccionadas
                int deleted = 0;
                for (int k = 0; k < _filtered_count; k++) {
                    if (_sel_map[k]) { vault_delete(_entry_names[_filtered[k]]); deleted++; }
                }
                _sel_mode = false;
                _sel_count = 0;
                memset(_sel_map, 0, sizeof(bool) * VAULT_MAX_ENTRIES);
                _batch_deleted_n = deleted;
                lv_obj_t* _ov = lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev)));
                lv_anim_del(_ov, NULL);
                lv_obj_del(_ov);
                lv_async_call([](void*){
                    _show_list(false);
                    toast_del(t(_batch_deleted_n == 1 ? K_TOAST_ENTRADA_ELIMINADA : K_TOAST_ENTRADAS_ELIMINADAS));
                }, nullptr);
            }, LV_EVENT_CLICKED, nullptr);
            return;
        }

        // Modo normal — abrir sheet de sort
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

        const int SH = 240;
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
        lv_label_set_text(sh_title, t(K_ORDENAR_VISTA));
        lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
        lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
        lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);

        _divider(sheet, 40);

        // Opciones de sort
        struct SortOpt { const char* label; int mode; };
        const SortOpt sorts[] = {
            { t(K_NOMBRE_AZ),      SORT_NAME_AZ   },
            { t(K_NOMBRE_ZA),      SORT_NAME_ZA   },
            { t(K_FECHA_RECIENTE),  SORT_DATE_DESC },
            { t(K_FECHA_ANTIGUA),   SORT_DATE_ASC  },
        };

        int sy = 48;
        for (int i = 0; i < 4; i++) {
            lv_obj_t* row = lv_obj_create(sheet);
            lv_obj_set_size(row, UI_W, 40);
            lv_obj_set_pos(row, 0, sy);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_set_style_radius(row, 0, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* lbl = lv_label_create(row);
            lv_label_set_text(lbl, sorts[i].label);
            lv_obj_set_style_text_font(lbl, FONT_MD, 0);
            lv_obj_set_style_text_color(lbl,
                _sort_mode == sorts[i].mode ? C_BTN_BG : C_TEXT, 0);
            lv_obj_set_pos(lbl, 16, 12);

            if (_sort_mode == sorts[i].mode) {
                lv_obj_t* chk = lv_label_create(row);
                lv_label_set_text(chk, ICO_CHECK); lv_obj_set_style_text_font(chk, FONT_ICON, 0);
                lv_obj_set_style_text_color(chk, C_BTN_BG, 0);
                lv_obj_set_style_text_font(chk, FONT_ICON, 0);
                lv_obj_set_pos(chk, UI_W - 28, 12);
            }

            int* mode_ptr = (int*)(intptr_t)sorts[i].mode;
            static int sort_modes[4];
            sort_modes[i] = sorts[i].mode;
            lv_obj_add_event_cb(row, [](lv_event_t* ev){
                int m = *(int*)lv_event_get_user_data(ev);
                _sort_mode = m;
                vault_set_sort(_sort_mode);
                lv_async_call([](void*){ _show_list(); }, nullptr);
            }, LV_EVENT_CLICKED, &sort_modes[i]);

            sy += 40;
        }



        // Animación subida
        lv_anim_t ash;
        lv_anim_init(&ash);
        lv_anim_set_var(&ash, sheet);
        lv_anim_set_exec_cb(&ash, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&ash, UI_H, UI_H - SH);
        lv_anim_set_time(&ash, 220);
        lv_anim_set_path_cb(&ash, lv_anim_path_ease_out);
        lv_anim_start(&ash);

    }, LV_EVENT_CLICKED, nullptr);

    // ── FAB expandido ─────────────────────────────────────────
    lv_obj_add_event_cb(fab, [](lv_event_t* e){
        if (_fab_open) return;
        _fab_open = true;

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
            _fab_open = false;
            lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
        }, LV_EVENT_CLICKED, overlay);

        const int SH = 180;
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
        lv_label_set_text(sh_title, t(K_NUEVA_ENTRADA));
        lv_obj_set_style_text_color(sh_title, C_TEXT, 0);
        lv_obj_set_style_text_font(sh_title, FONT_HDR, 0);
        lv_obj_align(sh_title, LV_ALIGN_TOP_MID, 0, 14);

        _divider(sheet, 40);

        // Opción: Cuenta web
        lv_obj_t* row_web = lv_obj_create(sheet);
        lv_obj_set_size(row_web, UI_W, 44);
        lv_obj_set_pos(row_web, 0, 46);
        lv_obj_set_style_bg_opa(row_web, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_web, 0, 0);
        lv_obj_set_style_pad_all(row_web, 0, 0);
        lv_obj_set_style_radius(row_web, 0, 0);
        lv_obj_clear_flag(row_web, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ico_web = lv_label_create(row_web);
        lv_label_set_text(ico_web, ICO_LOCK); lv_obj_set_style_text_font(ico_web, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico_web, C_TEXT, 0);
        lv_obj_set_style_text_font(ico_web, FONT_ICON, 0);
        lv_obj_set_pos(ico_web, 16, 14);
        lv_obj_clear_flag(ico_web, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* lbl_web = lv_label_create(row_web);
        lv_label_set_text(lbl_web, t(K_CUENTA));
        lv_obj_set_style_text_color(lbl_web, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl_web, FONT_MD, 0);
        lv_obj_set_pos(lbl_web, 42, 14);
        lv_obj_clear_flag(lbl_web, LV_OBJ_FLAG_CLICKABLE);
        _press_fb(row_web);
        lv_obj_add_event_cb(row_web, [](lv_event_t* ev){
            _fab_open = false;
            lv_obj_del(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev)));
            memset(_fw_name_buf, 0, sizeof(_fw_name_buf));
            memset(_fw_user_buf, 0, sizeof(_fw_user_buf));
            memset(_fw_pass_buf, 0, sizeof(_fw_pass_buf));
            _show_form_web(false);
        }, LV_EVENT_CLICKED, nullptr);

        // Divisor
        lv_obj_t* div = lv_obj_create(sheet);
        lv_obj_set_size(div, UI_W - 32, 1);
        lv_obj_set_pos(div, 16, 90);
        lv_obj_set_style_bg_color(div, C_BORDER, 0);
        lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(div, 0, 0);
        lv_obj_set_style_pad_all(div, 0, 0);

        // Opción: Secreto
        lv_obj_t* row_sec = lv_obj_create(sheet);
        lv_obj_set_size(row_sec, UI_W, 44);
        lv_obj_set_pos(row_sec, 0, 92);
        lv_obj_set_style_bg_opa(row_sec, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_sec, 0, 0);
        lv_obj_set_style_pad_all(row_sec, 0, 0);
        lv_obj_set_style_radius(row_sec, 0, 0);
        lv_obj_clear_flag(row_sec, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ico_sec = lv_label_create(row_sec);
        lv_label_set_text(ico_sec, ICO_PASSWORD); lv_obj_set_style_text_font(ico_sec, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico_sec, C_TEXT, 0);
        lv_obj_set_style_text_font(ico_sec, FONT_ICON, 0);
        lv_obj_set_pos(ico_sec, 16, 14);
        lv_obj_clear_flag(ico_sec, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* lbl_sec = lv_label_create(row_sec);
        lv_label_set_text(lbl_sec, t(K_OBJETO));
        lv_obj_set_style_text_color(lbl_sec, C_TEXT, 0);
        lv_obj_set_style_text_font(lbl_sec, FONT_MD, 0);
        lv_obj_set_pos(lbl_sec, 42, 14);
        lv_obj_clear_flag(lbl_sec, LV_OBJ_FLAG_CLICKABLE);
        _press_fb(row_sec);
        lv_obj_add_event_cb(row_sec, [](lv_event_t* ev){
            _fab_open = false;
            lv_obj_del(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev)));
            memset(_fs_name_buf,   0, sizeof(_fs_name_buf));
            memset(_fs_secret_buf, 0, sizeof(_fs_secret_buf));
            memset(_fs_cat_buf,    0, sizeof(_fs_cat_buf));
            memset(_fs_notes_buf,  0, sizeof(_fs_notes_buf));
            _show_form_secret(false);
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

    // ── Menú lateral ──────────────────────────────────────────
    lv_obj_add_event_cb(btn_menu, [](lv_event_t* e){
        if (_menu_open) return;
        _menu_open = true;

        lv_obj_t* scr_act = lv_scr_act();
        const int MENU_W = (UI_W * 2) / 3;

        lv_obj_t* overlay = lv_obj_create(scr_act);
        lv_obj_set_size(overlay, UI_W, UI_H);
        lv_obj_set_pos(overlay, 0, 0);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
        _fade_overlay(overlay, LV_OPA_50);
        lv_obj_set_style_border_width(overlay, 0, 0);
        lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* panel = lv_obj_create(overlay);
        lv_obj_set_size(panel, MENU_W, UI_H);
        lv_obj_set_pos(panel, -MENU_W, 0);
        lv_obj_set_style_bg_color(panel, C_BG, 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(panel, 0, 0);
        lv_obj_set_style_pad_all(panel, 0, 0);
        lv_obj_set_style_radius(panel, 0, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_add_event_cb(overlay, [](lv_event_t* ev){
            lv_obj_t* ov = (lv_obj_t*)lv_event_get_user_data(ev);
            _close_menu_overlay(ov);
        }, LV_EVENT_CLICKED, overlay);

        lv_obj_add_event_cb(panel, [](lv_event_t* ev){
            lv_event_stop_bubbling(ev);
        }, LV_EVENT_CLICKED, nullptr);

        _menu_panel_ref   = panel;
        _menu_overlay_ref = overlay;
        _menu_w = MENU_W;

        lv_anim_t am;
        lv_anim_init(&am);
        lv_anim_set_var(&am, panel);
        lv_anim_set_exec_cb(&am, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_set_values(&am, -MENU_W, 0);
        lv_anim_set_time(&am, 220);
        lv_anim_set_path_cb(&am, lv_anim_path_ease_out);
        lv_anim_start(&am);

        // ── Título Huskey Pocket ──────────────────────────────
        lv_obj_t* menu_title = lv_label_create(panel);
        lv_label_set_text(menu_title, "Huskey");
        lv_obj_set_style_text_color(menu_title, C_ACCENT, 0);
        lv_obj_set_style_text_font(menu_title, FONT_HDR_LG, 0);
        lv_obj_set_pos(menu_title, 16, 16);

        lv_obj_t* menu_sub = lv_label_create(panel);
        lv_label_set_text(menu_sub, "Pocket");
        lv_obj_set_style_text_color(menu_sub, C_MUTED, 0);
        lv_obj_set_style_text_font(menu_sub, FONT_MD, 0);
        lv_obj_set_pos(menu_sub, 16, 40);

        _divider(panel, 62);

        struct MenuOpt { const char* icon; const char* label; };
        const MenuOpt opts[] = {
            { ICO_HOME,      t(K_INICIO)        },
            { ICO_STAR,      t(K_FAVORITOS)     },
            { ICO_FOLDER,    t(K_CARPETAS)    },
            { ICO_AUTORENEW, "Generador"     },
            { ICO_SETTINGS,  t(K_AJUSTES) },
        };
        const int N_OPTS = 5;

        int my = 70;
        for (int i = 0; i < N_OPTS; i++) {
            lv_obj_t* opt_row = lv_obj_create(panel);
            lv_obj_set_size(opt_row, MENU_W - 16, 48);
            lv_obj_set_pos(opt_row, 8, my);
            lv_obj_set_style_bg_opa(opt_row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(opt_row, 0, 0);
            lv_obj_set_style_pad_all(opt_row, 0, 0);
            lv_obj_set_style_radius(opt_row, 10, 0);
            lv_obj_clear_flag(opt_row, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* ico = lv_label_create(opt_row);
            lv_label_set_text(ico, opts[i].icon);
            lv_obj_set_style_text_color(ico, C_TEXT, 0);
            lv_obj_set_style_text_font(ico, FONT_ICON, 0);
            lv_obj_align(ico, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_clear_flag(ico, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* lbl = lv_label_create(opt_row);
            lv_label_set_text(lbl, opts[i].label);
            lv_obj_set_style_text_color(lbl, C_TEXT, 0);
            lv_obj_set_style_text_font(lbl, FONT_MD, 0);
            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 42, 0);
            lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

            _press_fb(opt_row);

            // Pressed — resaltar fondo
            lv_obj_add_event_cb(opt_row, [](lv_event_t* ev){
                lv_obj_t* row = (lv_obj_t*)lv_event_get_target(ev);
                lv_obj_set_style_bg_color(row, C_BORDER, 0);
                lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            }, LV_EVENT_PRESSED, nullptr);

            // Press lost — dedo salió del área, cancelar
            lv_obj_add_event_cb(opt_row, [](lv_event_t* ev){
                lv_obj_t* row = (lv_obj_t*)lv_event_get_target(ev);
                lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
                lv_obj_clear_state(row, LV_STATE_PRESSED);
            }, LV_EVENT_PRESS_LOST, nullptr);

            // Released — solo ejecutar si el dedo soltó DENTRO
            lv_obj_add_event_cb(opt_row, [](lv_event_t* ev){
                lv_obj_t* row = (lv_obj_t*)lv_event_get_target(ev);
                // Verificar que el punto de release está dentro del objeto
                lv_point_t p;
                lv_indev_get_point(lv_indev_get_act(), &p);
                lv_area_t area;
                lv_obj_get_coords(row, &area);
                if (!_lv_area_is_point_on(&area, &p, 0)) {
                    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
                    return;
                }
                int action = (int)(intptr_t)lv_event_get_user_data(ev);
                vibro_pulse(); // CLICKED no se dispara con este manejo manual de press/release
                lv_obj_set_style_bg_color(row, C_ACCENT, 0);
                lv_obj_set_style_bg_opa(row, LV_OPA_30, 0);
                _close_menu_overlay(lv_obj_get_parent(lv_obj_get_parent(row))); // fila → panel → overlay
                switch(action) {
                    case 0:
                        // Si ya estamos en lista, solo cerrar panel
                        if (_current_screen != SCREEN_LIST) _show_list();
                        break;
                    case 1: _show_favorites();                 break;
                    case 2: _show_categories();                break;
                    case 3: _show_password_generator(nullptr); break;
                    case 4: _show_settings();                  break;
                }
            }, LV_EVENT_RELEASED, (void*)(intptr_t)i);

            my += 44;
        }

    }, LV_EVENT_CLICKED, nullptr);

    if (back) _load_screen_back(scr); else _load_screen_instant(scr);
}

// ── PANTALLA: Detalle ─────────────────────────────────────────
static bool _det_pass_visible = false;

#include "ui_qr.h"

static void _show_totp_setup(const char* name) {
    _current_screen = SCREEN_DETAIL; _kb_active = true;
    static char _totp_name[VAULT_FIELD_LEN];
    strncpy(_totp_name, name, VAULT_FIELD_LEN - 1);
    static char _totp_base[VAULT_FIELD_LEN] = {0};
    _totp_base[0] = '\0';

    lv_obj_t* scr = _make_screen();

    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_back = lv_btn_create(hdr);
    lv_obj_set_size(btn_back, 40, 44); lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0); lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back);
    lv_label_set_text(back_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(back_ico, C_TEXT, 0); lv_obj_center(back_ico);
    _press_fb(btn_back);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){
        lv_obj_t* ta_obj = kb_get_ta();
        const char* cur = ta_obj ? lv_textarea_get_text(ta_obj) : "";
        bool dirty = strcmp(cur, _totp_base) != 0;
        auto discard = []{ kb_reset(); _show_detail(_totp_name); };
        if (dirty) _show_discard_confirm(discard);
        else       discard();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, t(K_AGREGAR_2FA));
    lv_obj_set_style_text_color(title, C_TEXT, 0); lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    _divider(scr, 44);

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, t(K_SECRET_TOTP_DESC));
    lv_obj_set_style_text_color(lbl, C_MUTED, 0); lv_obj_set_style_text_font(lbl, FONT_SM, 0);
    lv_obj_set_width(lbl, UI_W - 32);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(lbl, 16, 50);

    lv_obj_t* ta = lv_textarea_create(scr);
    lv_obj_set_size(ta, UI_W - 32, 32);
    lv_obj_set_pos(ta, 16, 84);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "JBSWY3DPEHPK3PXP");
    {
        VaultEntry cur = vault_load(name);
        if (cur.valid && cur.totp_secret[0]) {
            lv_textarea_set_text(ta, cur.totp_secret);
            strncpy(_totp_base, cur.totp_secret, VAULT_FIELD_LEN - 1);
        }
    }
    lv_obj_set_style_bg_color(ta, C_CARD, 0); lv_obj_set_style_border_width(ta, 0, 0);
    lv_obj_set_style_radius(ta, 8, 0); lv_obj_set_style_text_color(ta, C_TEXT, 0);
    lv_obj_add_event_cb(ta, [](lv_event_t* e){ kb_set_ta((lv_obj_t*)lv_event_get_target(e)); kb_show(); }, LV_EVENT_FOCUSED, nullptr);

    lv_obj_t* btn_ok = lv_btn_create(hdr);
    lv_obj_set_size(btn_ok, 44, 44); lv_obj_set_pos(btn_ok, UI_W - 44, 0);
    lv_obj_set_style_bg_opa(btn_ok, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_ok, 0, 0); lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_t* ok_ico = lv_label_create(btn_ok);
    lv_label_set_text(ok_ico, ICO_CHECK); lv_obj_set_style_text_font(ok_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(ok_ico, C_ACCENT, 0); lv_obj_center(ok_ico);
    _press_fb(btn_ok);
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
        lv_obj_t* ta_obj = kb_get_ta();
        const char* secret = ta_obj ? lv_textarea_get_text(ta_obj) : "";
        if (strlen(secret) > 0) vault_set_totp_secret(_totp_name, secret);
        kb_reset();
        _show_detail(_totp_name);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* kb = kb_create(scr, ta, nullptr);
    lv_obj_set_pos(kb, 0, UI_H - kb_height());
    kb_set_ta(ta);

    _load_screen_instant(scr);
}

static void _show_notes_full(const char* notes) {
    static char _notes_ret_name[VAULT_FIELD_LEN];
    strncpy(_notes_ret_name, _selected_name, VAULT_FIELD_LEN - 1);
    _current_screen = SCREEN_DETAIL; _kb_active = false;

    lv_obj_t* scr = _make_screen();

    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 36); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_back = lv_btn_create(hdr);
    lv_obj_set_size(btn_back, 36, 36); lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0); lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back);
    lv_label_set_text(back_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(back_ico, C_TEXT, 0); lv_obj_center(back_ico);
    _press_fb(btn_back);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){ _show_detail(_notes_ret_name); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, t(K_NOTAS));
    lv_obj_set_style_text_color(title, C_TEXT, 0); lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    _divider(scr, 36);

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_W, UI_H - 36); lv_obj_set_pos(body, 0, 36);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 16, 0);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);

    lv_obj_t* n_val = lv_label_create(body);
    lv_label_set_text(n_val, notes);
    lv_obj_set_style_text_color(n_val, C_TEXT, 0); lv_obj_set_style_text_font(n_val, FONT_MD, 0);
    lv_label_set_long_mode(n_val, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(n_val, UI_W - 32);

    _load_screen_instant(scr);
}

static void _show_detail(const char* name) {
    _current_screen = SCREEN_DETAIL; _kb_active = false;
    VaultEntry e = vault_load(name);
    if (!e.valid) { _show_list(); return; }
    _det_pass_visible = false;
    // Entrada "solo nota": objeto sin secreto — sin card de secreto ni 2FA,
    // nota expandida y botones de acción más grandes.
    const bool note_only = (e.type != VAULT_TYPE_WEB) && e.secret[0] == '\0';

    lv_obj_t* scr = _make_screen();

    // ── Header ───────────────────────────────────────────────
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 32); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    // ← atrás
    lv_obj_t* btn_back = lv_btn_create(hdr);
    lv_obj_set_size(btn_back, 32, 32); lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0); lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back);
    lv_label_set_text(back_ico, ICO_ARROW_BACK); lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(back_ico, C_TEXT, 0); lv_obj_center(back_ico);
    _press_fb(btn_back);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){ _show_list(); }, LV_EVENT_CLICKED, nullptr);

    // Título centrado
    lv_obj_t* hdr_name = lv_label_create(hdr);
    lv_label_set_text(hdr_name, e.name);
    lv_obj_set_style_text_color(hdr_name, C_TEXT, 0);
    lv_obj_set_style_text_font(hdr_name, FONT_MD, 0);
    lv_obj_align(hdr_name, LV_ALIGN_CENTER, 0, 0);

    // 🗑 eliminar — esquina superior derecha
    lv_obj_t* btn_del_hdr = lv_btn_create(hdr);
    lv_obj_set_size(btn_del_hdr, 32, 32); lv_obj_set_pos(btn_del_hdr, UI_W - 32, 0);
    lv_obj_set_style_bg_opa(btn_del_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_del_hdr, 0, 0); lv_obj_set_style_border_width(btn_del_hdr, 0, 0);
    lv_obj_t* del_hdr_ico = lv_label_create(btn_del_hdr);
    lv_label_set_text(del_hdr_ico, ICO_DELETE); lv_obj_set_style_text_font(del_hdr_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(del_hdr_ico, C_DANGER, 0); lv_obj_center(del_hdr_ico);
    _press_fb(btn_del_hdr);
    lv_obj_add_event_cb(btn_del_hdr, [](lv_event_t* ev){
        lv_obj_t* overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
        _fade_overlay(overlay, LV_OPA_60);
        lv_obj_set_style_border_width(overlay, 0, 0); lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* box = lv_obj_create(overlay);
        lv_obj_set_size(box, UI_W - 48, 140); lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(box, C_CARD, 0); lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(box, 0, 0); lv_obj_set_style_radius(box, 12, 0);
        lv_obj_set_style_pad_all(box, 16, 0); lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(box, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* msg = lv_label_create(box);
        lv_label_set_text(msg, t(K_CONFIRMA_ELIMINAR_ENTRADA));
        lv_obj_set_style_text_color(msg, C_TEXT, 0); lv_obj_set_style_text_font(msg, FONT_MD, 0);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 0);
        int bw = (UI_W - 48 - 32 - 8) / 2;
        // Cancelar — izquierda
        lv_obj_t* btn_no = lv_btn_create(box);
        _press_fb(btn_no);
        lv_obj_set_size(btn_no, bw, 36); lv_obj_set_pos(btn_no, 0, 72);
        lv_obj_set_style_bg_color(btn_no, C_BORDER, 0); lv_obj_set_style_radius(btn_no, 8, 0);
        lv_obj_set_style_shadow_width(btn_no, 0, 0); lv_obj_set_style_border_width(btn_no, 1, 0);
        lv_obj_set_style_border_color(btn_no, C_MUTED, 0);
        lv_obj_t* no_lbl = lv_label_create(btn_no); lv_label_set_text(no_lbl, t(K_CANCELAR));
        lv_obj_set_style_text_color(no_lbl, C_TEXT, 0); lv_obj_set_style_text_font(no_lbl, FONT_SM, 0); lv_obj_center(no_lbl);
        lv_obj_add_event_cb(btn_no, [](lv_event_t* e){
            lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e))));
        }, LV_EVENT_CLICKED, nullptr);
        // Eliminar — derecha
        lv_obj_t* btn_ok = lv_btn_create(box);
        _press_fb(btn_ok);
        lv_obj_set_size(btn_ok, bw, 36); lv_obj_set_pos(btn_ok, bw + 8, 72);
        lv_obj_set_style_bg_color(btn_ok, C_DANGER, 0); lv_obj_set_style_radius(btn_ok, 8, 0);
        lv_obj_set_style_shadow_width(btn_ok, 0, 0); lv_obj_set_style_border_width(btn_ok, 0, 0);
        lv_obj_t* ok_lbl = lv_label_create(btn_ok); lv_label_set_text(ok_lbl, t(K_ELIMINAR));
        lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(ok_lbl, FONT_SM, 0); lv_obj_center(ok_lbl);
        lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
            vault_delete(_selected_name);
            lv_obj_t* _ov = lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e)));
            lv_anim_del(_ov, NULL);
            lv_obj_del(_ov);
            lv_async_call([](void*){
                _show_list();
                toast_del(t(K_TOAST_ENTRADA_ELIMINADA));
            }, nullptr);
        }, LV_EVENT_CLICKED, nullptr);
    }, LV_EVENT_CLICKED, nullptr);

    // ⭐ favorito
    lv_obj_t* btn_fav = lv_btn_create(hdr);
    _press_fb(btn_fav);
    lv_obj_set_size(btn_fav, 32, 32); lv_obj_set_pos(btn_fav, UI_W - 64, 0);
    lv_obj_set_style_bg_opa(btn_fav, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_fav, 0, 0); lv_obj_set_style_border_width(btn_fav, 0, 0);
    lv_obj_t* fav_ico = lv_label_create(btn_fav);
    lv_label_set_text(fav_ico, ICO_STAR); lv_obj_set_style_text_font(fav_ico, FONT_ICON, 0);
    bool is_fav = vault_get_favorite(e.name);
    lv_obj_set_style_text_color(fav_ico, is_fav ? lv_color_hex(0xFFCC00) : C_MUTED, 0);
    lv_obj_center(fav_ico);
    lv_obj_add_event_cb(btn_fav, [](lv_event_t* ev){
        bool cur = vault_get_favorite(_selected_name);
        vault_set_favorite(_selected_name, !cur);
        lv_obj_t* ico = lv_obj_get_child((lv_obj_t*)lv_event_get_target(ev), 0);
        lv_obj_set_style_text_color(ico, !cur ? lv_color_hex(0xFFCC00) : C_MUTED, 0);
    }, LV_EVENT_CLICKED, nullptr);

    _divider(scr, 32);

    // ── Cuerpo scrolleable ───────────────────────────────────
    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_size(body, UI_W, UI_H - 32); lv_obj_set_pos(body, 0, 32);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_top(body, 2, 0);
    lv_obj_set_style_pad_left(body, 10, 0); lv_obj_set_style_pad_right(body, 10, 0);
    lv_obj_set_style_pad_row(body, 3, 0);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_bottom(body, 6, 0);

    static char _det_pass_buf[VAULT_FIELD_LEN];
    static char _det_sec_buf[VAULT_FIELD_LEN];
    strncpy(_det_pass_buf, e.password, VAULT_FIELD_LEN-1);
    strncpy(_det_sec_buf,  e.secret,   VAULT_FIELD_LEN-1);

    // ── Card de datos ────────────────────────────────────────
    lv_obj_t* card = lv_obj_create(body);
    lv_obj_set_size(card, UI_W - 24, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, C_CARD, 0); lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0); lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_top(card, 3, 0);
    lv_obj_set_style_pad_bottom(card, 3, 0);
    lv_obj_set_style_pad_left(card, 14, 0);
    lv_obj_set_style_pad_right(card, 14, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (e.type == VAULT_TYPE_WEB) {
        static char _det_user_buf[VAULT_FIELD_LEN];
        strncpy(_det_user_buf, e.username, VAULT_FIELD_LEN-1);

        // Usuario
        lv_obj_t* u_lbl = lv_label_create(card);
        lv_label_set_text(u_lbl, t(K_USUARIO));
        lv_obj_set_style_text_color(u_lbl, C_MUTED, 0); lv_obj_set_style_text_font(u_lbl, FONT_SM, 0);
        lv_obj_set_pos(u_lbl, 0, 0);

        lv_obj_t* u_val = lv_label_create(card);
        lv_label_set_text(u_val, e.username);
        lv_obj_set_style_text_color(u_val, C_TEXT, 0); lv_obj_set_style_text_font(u_val, FONT_MD, 0);
        lv_label_set_long_mode(u_val, LV_LABEL_LONG_DOT);
        lv_obj_set_width(u_val, UI_W - 52);
        lv_obj_set_pos(u_val, 0, 14);

        // Divisor
        lv_obj_t* div1 = lv_obj_create(card);
        lv_obj_set_size(div1, UI_W - 52, 1); lv_obj_set_pos(div1, 0, 36);
        lv_obj_set_style_bg_color(div1, C_BORDER, 0); lv_obj_set_style_bg_opa(div1, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(div1, 0, 0);

        // Contraseña
        lv_obj_t* p_lbl = lv_label_create(card);
        lv_label_set_text(p_lbl, t(K_CONTRASENA));
        lv_obj_set_style_text_color(p_lbl, C_MUTED, 0); lv_obj_set_style_text_font(p_lbl, FONT_SM, 0);
        lv_obj_set_pos(p_lbl, 0, 44);

        lv_obj_t* p_val = lv_label_create(card);
        lv_label_set_text(p_val, "••••••••••••");
        lv_obj_set_style_text_color(p_val, C_TEXT, 0); lv_obj_set_style_text_font(p_val, FONT_MD, 0);
        lv_label_set_long_mode(p_val, LV_LABEL_LONG_DOT);
        lv_obj_set_width(p_val, UI_W - 80);
        lv_obj_set_pos(p_val, 0, 58);

        // Botón ojo — área táctil más grande
        lv_obj_t* eye = lv_btn_create(card);
        _press_fb(eye);
        lv_obj_set_size(eye, 40, 40); lv_obj_set_pos(eye, UI_W - 80, 48);
        lv_obj_set_style_bg_opa(eye, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(eye, 0, 0); lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_t* eye_ico = lv_label_create(eye);
        lv_label_set_text(eye_ico, ICO_VISIBILITY);
        lv_obj_set_style_text_font(eye_ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(eye_ico, C_MUTED, 0); lv_obj_center(eye_ico);
        struct PassED { lv_obj_t* val; lv_obj_t* ico; };
        static PassED ped; ped = {p_val, eye_ico};
        lv_obj_add_event_cb(eye, [](lv_event_t* ev){
            PassED* d = (PassED*)lv_event_get_user_data(ev);
            _det_pass_visible = !_det_pass_visible;
            lv_label_set_text(d->val, _det_pass_visible ? _det_pass_buf : "••••••••••••");
            // Ojo abierto = oculto, ojo cerrado = visible
            lv_label_set_text(d->ico, _det_pass_visible ? ICO_VISIBILITY_OFF : ICO_VISIBILITY);
        }, LV_EVENT_CLICKED, &ped);

        lv_obj_set_height(card, 84);

    } else if (e.secret[0] == '\0') {
        // Entrada "solo nota": sin secreto que mostrar — ocultar la card
        lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Secreto
        lv_obj_t* s_lbl = lv_label_create(card);
        lv_label_set_text(s_lbl, t(K_SECRETO));
        lv_obj_set_style_text_color(s_lbl, C_MUTED, 0); lv_obj_set_style_text_font(s_lbl, FONT_SM, 0);
        lv_obj_set_pos(s_lbl, 0, 0);

        lv_obj_t* s_val = lv_label_create(card);
        lv_label_set_text(s_val, "••••••••••••");
        lv_obj_set_style_text_color(s_val, C_TEXT, 0); lv_obj_set_style_text_font(s_val, FONT_MD, 0);
        lv_label_set_long_mode(s_val, LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_val, UI_W - 80);
        lv_obj_set_pos(s_val, 0, 16);

        lv_obj_t* eye2 = lv_btn_create(card);
        _press_fb(eye2);
        lv_obj_set_size(eye2, 28, 28); lv_obj_set_pos(eye2, UI_W - 72, 12);
        lv_obj_set_style_bg_opa(eye2, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(eye2, 0, 0); lv_obj_set_style_border_width(eye2, 0, 0);
        lv_obj_t* eye2_ico = lv_label_create(eye2);
        lv_label_set_text(eye2_ico, ICO_VISIBILITY);
        lv_obj_set_style_text_font(eye2_ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(eye2_ico, C_MUTED, 0); lv_obj_center(eye2_ico);
        struct SecED { lv_obj_t* val; lv_obj_t* ico; };
        static SecED sed; sed = {s_val, eye2_ico};
        lv_obj_add_event_cb(eye2, [](lv_event_t* ev){
            SecED* d = (SecED*)lv_event_get_user_data(ev);
            _det_pass_visible = !_det_pass_visible;
            lv_label_set_text(d->val, _det_pass_visible ? _det_sec_buf : "••••••••••••");
            lv_label_set_text(d->ico, _det_pass_visible ? ICO_VISIBILITY_OFF : ICO_VISIBILITY);
        }, LV_EVENT_CLICKED, &sed);

        lv_obj_set_height(card, 48);
    }

    // ── Notas (solo si hay algo escrito) ─────────────────────
    if (strlen(e.notes) > 0) {
        lv_obj_t* notes_card = lv_obj_create(body);
        lv_obj_set_size(notes_card, UI_W - 24, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(notes_card, C_CARD, 0); lv_obj_set_style_bg_opa(notes_card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(notes_card, 0, 0); lv_obj_set_style_radius(notes_card, 12, 0);
        lv_obj_set_style_pad_top(notes_card, 4, 0); lv_obj_set_style_pad_bottom(notes_card, 4, 0);
        lv_obj_set_style_pad_left(notes_card, 14, 0); lv_obj_set_style_pad_right(notes_card, 14, 0);
        lv_obj_clear_flag(notes_card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(notes_card, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* n_lbl = lv_label_create(notes_card);
        lv_label_set_text(n_lbl, t(K_NOTAS));
        lv_obj_set_style_text_color(n_lbl, C_MUTED, 0); lv_obj_set_style_text_font(n_lbl, FONT_SM, 0);
        lv_obj_set_pos(n_lbl, 0, 0);
        lv_obj_clear_flag(n_lbl, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* n_val = lv_label_create(notes_card);
        lv_label_set_text(n_val, e.notes);
        lv_obj_set_style_text_color(n_val, C_TEXT, 0); lv_obj_set_style_text_font(n_val, FONT_MD, 0);
        if (note_only) {
            // Solo nota: la nota es el contenido principal — multilínea,
            // crece con el texto hasta ~6 líneas (el tap sigue abriendo la
            // vista completa para notas más largas).
            lv_label_set_long_mode(n_val, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(n_val, UI_W - 52);
            lv_obj_set_height(n_val, LV_SIZE_CONTENT);
            lv_obj_set_style_max_height(n_val, 96, 0);
        } else {
            lv_label_set_long_mode(n_val, LV_LABEL_LONG_DOT); // una línea + "…"
            lv_obj_set_size(n_val, UI_W - 52, 16); // altura fija de 1 línea — sin esto LONG_DOT no trunca
        }
        lv_obj_set_pos(n_val, 0, 15);
        lv_obj_clear_flag(n_val, LV_OBJ_FLAG_CLICKABLE);

        static char _notes_full[VAULT_NOTES_LEN];
        strncpy(_notes_full, e.notes, VAULT_NOTES_LEN - 1);
        lv_obj_add_event_cb(notes_card, [](lv_event_t* ev){ _show_notes_full(_notes_full); }, LV_EVENT_CLICKED, nullptr);
    }

    // ── Editar / Compartir QR — fila compacta, solo iconos ────
    lv_obj_t* icon_row = lv_obj_create(body);
    const int icon_h = note_only ? 40 : 28; // solo-nota: botones más cómodos
    lv_obj_set_size(icon_row, UI_W - 24, icon_h);
    lv_obj_set_style_bg_opa(icon_row, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(icon_row, 0, 0);
    lv_obj_set_style_pad_all(icon_row, 0, 0); lv_obj_clear_flag(icon_row, LV_OBJ_FLAG_SCROLLABLE);

    int icw = (UI_W - 24 - 8) / 2;
    lv_obj_t* btn_edit = lv_btn_create(icon_row);
    lv_obj_set_size(btn_edit, icw, icon_h); lv_obj_set_pos(btn_edit, 0, 0);
    lv_obj_set_style_bg_color(btn_edit, C_CARD, 0); lv_obj_set_style_bg_opa(btn_edit, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_edit, 0, 0); lv_obj_set_style_radius(btn_edit, 10, 0);
    lv_obj_set_style_shadow_width(btn_edit, 0, 0);
    _press_fb(btn_edit);
    lv_obj_t* edit_ico = lv_label_create(btn_edit);
    lv_label_set_text(edit_ico, ICO_EDIT); lv_obj_set_style_text_font(edit_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(edit_ico, C_ACCENT, 0); lv_obj_center(edit_ico);
    lv_obj_clear_flag(edit_ico, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_edit, [](lv_event_t* ev){
        VaultEntry en = vault_load(_selected_name);
        if (en.type == VAULT_TYPE_WEB) _show_form_web(true);
        else _show_form_secret(true);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_qr = lv_btn_create(icon_row);
    lv_obj_set_size(btn_qr, icw, icon_h); lv_obj_set_pos(btn_qr, icw + 8, 0);
    lv_obj_set_style_bg_color(btn_qr, C_CARD, 0); lv_obj_set_style_bg_opa(btn_qr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_qr, 0, 0); lv_obj_set_style_radius(btn_qr, 10, 0);
    lv_obj_set_style_shadow_width(btn_qr, 0, 0);
    _press_fb(btn_qr);
    lv_obj_t* qr_ico = lv_label_create(btn_qr);
    lv_label_set_text(qr_ico, ICO_QR_CODE); lv_obj_set_style_text_font(qr_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(qr_ico, C_TEXT, 0); lv_obj_center(qr_ico);
    lv_obj_clear_flag(qr_ico, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_qr, [](lv_event_t* ev){
        VaultEntry en = vault_load(_selected_name);
        if (!en.valid) return;
        static char qr_buf[400];
        int pos = 0;
        if (en.type == VAULT_TYPE_WEB) {
            pos += snprintf(qr_buf + pos, sizeof(qr_buf) - pos, "Usuario: %s\n", en.username);
            pos += snprintf(qr_buf + pos, sizeof(qr_buf) - pos, "Contraseña: %s\n", en.password);
        } else if (en.secret[0]) {
            pos += snprintf(qr_buf + pos, sizeof(qr_buf) - pos, "Secreto: %s\n", en.secret);
        }
        if (strlen(en.notes) > 0) {
            pos += snprintf(qr_buf + pos, sizeof(qr_buf) - pos, "Notas: %s\n", en.notes);
        }
        _show_qr_share(en.name, qr_buf);
    }, LV_EVENT_CLICKED, nullptr);

    if (e.type == VAULT_TYPE_WEB) {
        static char _det_user_buf2[VAULT_FIELD_LEN];
        strncpy(_det_user_buf2, e.username, VAULT_FIELD_LEN-1);

        // ── Divisor con texto ────────────────────────────────
        lv_obj_t* kb_div = lv_obj_create(body);
        lv_obj_set_size(kb_div, UI_W - 24, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(kb_div, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(kb_div, 0, 0);
        lv_obj_set_style_pad_all(kb_div, 0, 0); lv_obj_clear_flag(kb_div, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* div_lbl = lv_label_create(kb_div);
        lv_label_set_text(div_lbl, t(K_ESCRIBIR_TECLADO_FISICO));
        lv_obj_set_style_text_color(div_lbl, C_MUTED, 0); lv_obj_set_style_text_font(div_lbl, FONT_SM, 0);
        lv_obj_align(div_lbl, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_height(kb_div, 12);

        // Fila 1: Escribir usuario + Tab
        lv_obj_t* row1 = lv_obj_create(body);
        lv_obj_set_size(row1, UI_W - 24, 28);
        lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row1, 0, 0);
        lv_obj_set_style_pad_all(row1, 0, 0); lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);

        int kbw = (UI_W - 24 - 8) / 2;  // ancho igual para todos los botones
        lv_obj_t* btn_usr = lv_btn_create(row1);
        lv_obj_set_size(btn_usr, kbw, 28); lv_obj_set_pos(btn_usr, 0, 0);
        lv_obj_set_style_bg_color(btn_usr, C_CARD, 0); lv_obj_set_style_radius(btn_usr, 10, 0);
        lv_obj_set_style_shadow_width(btn_usr, 0, 0); lv_obj_set_style_border_width(btn_usr, 0, 0);
        _press_fb(btn_usr);
        lv_obj_t* usr_row = lv_obj_create(btn_usr);
        lv_obj_set_size(usr_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT); lv_obj_align(usr_row, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(usr_row, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(usr_row, 0, 0);
        lv_obj_set_style_pad_all(usr_row, 0, 0); lv_obj_set_style_pad_column(usr_row, 6, 0);
        lv_obj_set_flex_flow(usr_row, LV_FLEX_FLOW_ROW); lv_obj_clear_flag(usr_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(usr_row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* ui = lv_label_create(usr_row); lv_label_set_text(ui, ICO_PERSON);
        lv_obj_set_style_text_font(ui, FONT_ICON_EX, 0);
        lv_obj_set_style_text_color(ui, C_TEXT, 0); lv_obj_clear_flag(ui, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* ut = lv_label_create(usr_row); lv_label_set_text(ut, t(K_USUARIO));
        lv_obj_set_style_text_font(ut, FONT_SM, 0);
        lv_obj_set_style_text_color(ut, C_TEXT, 0); lv_obj_clear_flag(ut, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn_usr, [](lv_event_t* ev){ kb_type_user(_det_user_buf2); }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* btn_tab = lv_btn_create(row1);
        lv_obj_set_size(btn_tab, kbw, 28); lv_obj_set_pos(btn_tab, kbw + 8, 0);
        lv_obj_set_style_bg_color(btn_tab, C_CARD, 0); lv_obj_set_style_radius(btn_tab, 10, 0);
        lv_obj_set_style_shadow_width(btn_tab, 0, 0); lv_obj_set_style_border_width(btn_tab, 0, 0);
        _press_fb(btn_tab);
        lv_obj_t* tab_lbl = lv_label_create(btn_tab); lv_label_set_text(tab_lbl, "Tab");
        lv_obj_set_style_text_font(tab_lbl, FONT_SM, 0);
        lv_obj_set_style_text_color(tab_lbl, C_TEXT, 0); lv_obj_center(tab_lbl);
        lv_obj_add_event_cb(btn_tab, [](lv_event_t* ev){ kb_type_tab(); }, LV_EVENT_CLICKED, nullptr);

        // Fila 2: Escribir contraseña + Enter
        lv_obj_t* row2 = lv_obj_create(body);
        lv_obj_set_size(row2, UI_W - 24, 28);
        lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row2, 0, 0);
        lv_obj_set_style_pad_all(row2, 0, 0); lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* btn_pw = lv_btn_create(row2);
        lv_obj_set_size(btn_pw, kbw, 28); lv_obj_set_pos(btn_pw, 0, 0);
        lv_obj_set_style_bg_color(btn_pw, C_CARD, 0); lv_obj_set_style_radius(btn_pw, 10, 0);
        lv_obj_set_style_shadow_width(btn_pw, 0, 0); lv_obj_set_style_border_width(btn_pw, 0, 0);
        _press_fb(btn_pw);
        lv_obj_t* pw_row = lv_obj_create(btn_pw);
        lv_obj_set_size(pw_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT); lv_obj_align(pw_row, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(pw_row, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(pw_row, 0, 0);
        lv_obj_set_style_pad_all(pw_row, 0, 0); lv_obj_set_style_pad_column(pw_row, 6, 0);
        lv_obj_set_flex_flow(pw_row, LV_FLEX_FLOW_ROW); lv_obj_clear_flag(pw_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(pw_row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* pi = lv_label_create(pw_row); lv_label_set_text(pi, ICO_PASSWORD);
        lv_obj_set_style_text_font(pi, FONT_ICON, 0);
        lv_obj_set_style_text_color(pi, C_TEXT, 0); lv_obj_clear_flag(pi, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* pt = lv_label_create(pw_row); lv_label_set_text(pt, t(K_CONTRASENA));
        lv_obj_set_style_text_font(pt, FONT_SM, 0);
        lv_obj_set_style_text_color(pt, C_TEXT, 0); lv_obj_clear_flag(pt, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn_pw, [](lv_event_t* ev){ kb_type_pass(_det_pass_buf); }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* btn_enter = lv_btn_create(row2);
        lv_obj_set_size(btn_enter, kbw, 28); lv_obj_set_pos(btn_enter, kbw + 8, 0);
        lv_obj_set_style_bg_color(btn_enter, C_CARD, 0); lv_obj_set_style_radius(btn_enter, 10, 0);
        lv_obj_set_style_shadow_width(btn_enter, 0, 0); lv_obj_set_style_border_width(btn_enter, 0, 0);
        _press_fb(btn_enter);
        lv_obj_t* enter_lbl = lv_label_create(btn_enter); lv_label_set_text(enter_lbl, "Enter");
        lv_obj_set_style_text_font(enter_lbl, FONT_SM, 0);
        lv_obj_set_style_text_color(enter_lbl, C_TEXT, 0); lv_obj_center(enter_lbl);
        lv_obj_add_event_cb(btn_enter, [](lv_event_t* ev){ kb_type_enter(); }, LV_EVENT_CLICKED, nullptr);
    }

    // ── TOTP / 2FA — al final, tras los botones de teclado ────
    // No aplica a entradas "solo nota" (sin secreto no hay cuenta que
    // proteger con 2FA — el botón solo confundiría).
    if (!note_only) {
    lv_obj_t* totp_card = lv_obj_create(body);
    lv_obj_set_size(totp_card, UI_W - 24, 46);
    lv_obj_set_style_bg_color(totp_card, C_CARD, 0); lv_obj_set_style_bg_opa(totp_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(totp_card, 0, 0); lv_obj_set_style_radius(totp_card, 12, 0);
    lv_obj_set_style_pad_all(totp_card, 0, 0);
    lv_obj_clear_flag(totp_card, LV_OBJ_FLAG_SCROLLABLE);

    if (e.totp_secret[0]) {
        // Bloque texto (label + código) centrado verticalmente como
        // grupo — LV_SIZE_CONTENT + align CENTER en vez de offsets fijos.
        lv_obj_t* t_group = lv_obj_create(totp_card);
        lv_obj_set_size(t_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(t_group, LV_ALIGN_LEFT_MID, 14, 0);
        lv_obj_set_style_bg_opa(t_group, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(t_group, 0, 0);
        lv_obj_set_style_pad_all(t_group, 0, 0); lv_obj_clear_flag(t_group, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(t_group, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* t_lbl = lv_label_create(t_group);
        lv_label_set_text(t_lbl, t(K_CODIGO_2FA));
        lv_obj_set_style_text_color(t_lbl, C_MUTED, 0); lv_obj_set_style_text_font(t_lbl, FONT_SM, 0);
        lv_obj_set_pos(t_lbl, 0, 0);
        lv_obj_clear_flag(t_lbl, LV_OBJ_FLAG_CLICKABLE);

        int secs_left = 30;
        int code = vault_totp_now(e.name, &secs_left);
        char code_str[8];
        if (code >= 0) snprintf(code_str, sizeof(code_str), "%03d %03d", code / 1000, code % 1000);
        else            snprintf(code_str, sizeof(code_str), "------");

        lv_obj_t* t_val = lv_label_create(t_group);
        lv_label_set_text(t_val, code_str);
        lv_obj_set_style_text_color(t_val, C_TEXT, 0); lv_obj_set_style_text_font(t_val, FONT_HDR, 0);
        lv_obj_set_pos(t_val, 0, 15);
        lv_obj_clear_flag(t_val, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* t_timer_lbl = lv_label_create(totp_card);
        char sec_str[8]; snprintf(sec_str, sizeof(sec_str), "%ds", secs_left);
        lv_label_set_text(t_timer_lbl, sec_str);
        lv_obj_set_style_text_color(t_timer_lbl, C_MUTED, 0); lv_obj_set_style_text_font(t_timer_lbl, FONT_SM, 0);
        lv_obj_align(t_timer_lbl, LV_ALIGN_RIGHT_MID, -56, 0);
        lv_obj_clear_flag(t_timer_lbl, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* btn_type = lv_btn_create(totp_card);
        lv_obj_set_size(btn_type, 32, 32);
        lv_obj_align(btn_type, LV_ALIGN_RIGHT_MID, -14, 0);
        lv_obj_set_style_bg_color(btn_type, C_ACCENT, 0);
        lv_obj_set_style_radius(btn_type, 8, 0);
        lv_obj_set_style_shadow_width(btn_type, 0, 0);
        lv_obj_t* type_ico = lv_label_create(btn_type);
        lv_label_set_text(type_ico, ICO_KEYBOARD);
        lv_obj_set_style_text_font(type_ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(type_ico, C_ACCENT_TEXT, 0);
        lv_obj_center(type_ico);
        lv_obj_add_flag(btn_type, LV_OBJ_FLAG_CLICKABLE);
        static char _totp_type_name[VAULT_FIELD_LEN];
        strncpy(_totp_type_name, e.name, VAULT_FIELD_LEN - 1);
        lv_obj_add_event_cb(btn_type, [](lv_event_t* ev){
            lv_event_stop_bubbling(ev); // no disparar el tap-para-editar de la card
            kb_type_totp(_totp_type_name);
        }, LV_EVENT_CLICKED, nullptr);

        // Refresco periódico del código TOTP y cuenta regresiva
        struct TotpUI { lv_obj_t* val; lv_obj_t* timer_lbl; char name[VAULT_FIELD_LEN]; };
        TotpUI* tui = new TotpUI();
        tui->val = t_val; tui->timer_lbl = t_timer_lbl;
        strncpy(tui->name, e.name, VAULT_FIELD_LEN - 1);
        lv_timer_create([](lv_timer_t* tm){
            TotpUI* d = (TotpUI*)tm->user_data;
            if (!lv_obj_is_valid(d->val)) { lv_timer_del(tm); delete d; return; }
            int sl = 30;
            int c = vault_totp_now(d->name, &sl);
            char cs[8];
            if (c >= 0) snprintf(cs, sizeof(cs), "%03d %03d", c / 1000, c % 1000);
            else         snprintf(cs, sizeof(cs), "------");
            lv_label_set_text(d->val, cs);
            char ss[8]; snprintf(ss, sizeof(ss), "%ds", sl);
            lv_label_set_text(d->timer_lbl, ss);
        }, 1000, tui);

        lv_obj_add_flag(totp_card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(totp_card, [](lv_event_t* ev){ _show_totp_setup(_selected_name); }, LV_EVENT_CLICKED, nullptr);
    } else {
        lv_obj_t* row = lv_obj_create(totp_card);
        lv_obj_set_size(row, LV_SIZE_CONTENT, 24); // ancho del contenido para poder centrar
        lv_obj_align(row, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0); lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* plus_ico = lv_label_create(row);
        lv_label_set_text(plus_ico, ICO_ADD); lv_obj_set_style_text_font(plus_ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(plus_ico, C_ACCENT, 0); lv_obj_set_pos(plus_ico, 0, 0);
        lv_obj_clear_flag(plus_ico, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* plus_lbl = lv_label_create(row);
        lv_label_set_text(plus_lbl, t(K_AGREGAR_2FA));
        lv_obj_set_style_text_color(plus_lbl, C_ACCENT, 0); lv_obj_set_style_text_font(plus_lbl, FONT_MD, 0);
        lv_obj_set_pos(plus_lbl, 28, 2);
        lv_obj_clear_flag(plus_lbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(totp_card, [](lv_event_t* ev){ _show_totp_setup(_selected_name); }, LV_EVENT_CLICKED, nullptr);
    }
    } // !note_only

    _load_screen(scr);
}


// ── PANTALLA: Favoritos ───────────────────────────────────────
static void _show_favorites() {
    _current_screen = SCREEN_FAVORITES; _kb_active = false;
    lv_obj_t* scr = _make_screen();

    // ── Header ────────────────────────────────────────────────
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, UI_W, 44); lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_BG, 0); lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0); lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bb = lv_btn_create(hdr);
    lv_obj_set_size(bb, 40, 44); lv_obj_set_pos(bb, 0, 0);
    lv_obj_set_style_bg_opa(bb, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(bb, 0, 0);
    lv_obj_set_style_border_width(bb, 0, 0);
    lv_obj_t* bi = lv_label_create(bb);
    lv_label_set_text(bi, ICO_ARROW_BACK);
    lv_obj_set_style_text_font(bi, FONT_ICON, 0);
    lv_obj_set_style_text_color(bi, C_TEXT, 0);
    lv_obj_center(bi);
    _press_fb(bb);
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_list(); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* tt = lv_label_create(hdr);
    lv_label_set_text(tt, t(K_FAVORITOS));
    lv_obj_set_style_text_color(tt, C_TEXT, 0);
    lv_obj_set_style_text_font(tt, FONT_MD, 0);
    lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);

    _divider(scr, 44);

    // ── Lista ────────────────────────────────────────────────
    // Buffer en PSRAM referenciado por callbacks de la lista
    static char (*all_names)[VAULT_FIELD_LEN] = nullptr;
    if (!all_names) all_names = (char(*)[VAULT_FIELD_LEN]) ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES);
    int all_count = 0;
    if (all_names) vault_list(all_names, &all_count, VAULT_MAX_ENTRIES);

    lv_obj_t* list = lv_obj_create(scr);
    lv_obj_set_size(list, UI_W, UI_H - 44);
    lv_obj_set_pos(list, 0, 44);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 8, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    int fav_count = 0;
    for (int i = 0; i < all_count; i++) {
        if (!vault_get_favorite(all_names[i])) continue;
        fav_count++;

        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, UI_W - 16, 46);
        lv_obj_set_style_bg_color(row, C_CARD, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        _press_fb(row);

        lv_obj_t* name_lbl = lv_label_create(row);
        lv_label_set_text(name_lbl, all_names[i]);
        lv_obj_set_style_text_color(name_lbl, C_TEXT, 0);
        lv_obj_set_style_text_font(name_lbl, FONT_MD, 0);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name_lbl, UI_W - 64);
        lv_obj_set_pos(name_lbl, 12, 6);

        uint8_t ty = vault_get_type(all_names[i]);
        VaultEntry ve = vault_load(all_names[i]);
        lv_obj_t* sub = lv_label_create(row);
        if (ty == VAULT_TYPE_WEB && ve.valid) lv_label_set_text(sub, ve.username);
        else lv_label_set_text(sub, t(K_OBJETO));
        lv_obj_set_style_text_color(sub, C_MUTED, 0);
        lv_obj_set_style_text_font(sub, FONT_SM, 0);
        lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
        lv_obj_set_width(sub, UI_W - 64);
        lv_obj_set_pos(sub, 12, 26);

        lv_obj_t* star = lv_label_create(row);
        lv_label_set_text(star, ICO_STAR);
        lv_obj_set_style_text_font(star, FONT_ICON, 0);
        lv_obj_set_style_text_color(star, lv_color_hex(0xFFCC00), 0);
        lv_obj_set_pos(star, UI_W - 52, 14);
        lv_obj_clear_flag(star, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* arr = lv_label_create(row);
        lv_label_set_text(arr, ICO_CHEVRON_RIGHT);
        lv_obj_set_style_text_font(arr, FONT_ICON, 0);
        lv_obj_set_style_text_color(arr, C_MUTED, 0);
        lv_obj_set_pos(arr, UI_W - 36, 14);
        lv_obj_clear_flag(arr, LV_OBJ_FLAG_CLICKABLE);

        char* np = all_names[i];
        lv_obj_add_event_cb(row, [](lv_event_t* e){
            char* n = (char*)lv_event_get_user_data(e);
            strncpy(_selected_name, n, VAULT_FIELD_LEN-1);
            _show_detail(_selected_name);
        }, LV_EVENT_CLICKED, np);
    }

    if (fav_count == 0) {
        lv_obj_t* empty = lv_label_create(scr);
        lv_label_set_text(empty, t(K_SIN_FAVORITOS));
        lv_obj_set_style_text_color(empty, C_MUTED, 0);
        lv_obj_set_style_text_font(empty, FONT_SM, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);
    }
    _load_screen(scr);
}

// ── PANTALLA: Detalle de categoría ───────────────────────────
static char _cur_cat_id[VAULT_CAT_LEN];
static char _cur_cat_name[VAULT_CAT_LEN];

static void _show_category_detail(const char* cat_id, const char* cat_name) {
    _current_screen = SCREEN_CAT_DETAIL; _kb_active = false;
    strncpy(_cur_cat_id,   cat_id,   VAULT_CAT_LEN-1);
    strncpy(_cur_cat_name, cat_name, VAULT_CAT_LEN-1);

    // Buffer en PSRAM referenciado por callbacks de la lista
    static char (*names)[VAULT_FIELD_LEN] = nullptr;
    if (!names) names = (char(*)[VAULT_FIELD_LEN]) ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES);
    int count = 0;
    if (names) vault_list(names, &count, VAULT_MAX_ENTRIES, cat_id);

    lv_obj_t* scr = _make_screen();

    // ── Header ────────────────────────────────────────────────
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
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_categories(true); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* tt = lv_label_create(hdr);
    lv_label_set_text(tt, cat_name);
    lv_obj_set_style_text_color(tt, C_TEXT, 0);
    lv_obj_set_style_text_font(tt, FONT_MD, 0);
    lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);

    // Botón renombrar — solo si no es CAT_NONE
    lv_obj_t* btn_ren = lv_btn_create(hdr);
    if (strcmp(cat_id, CAT_NONE) == 0) lv_obj_add_flag(btn_ren, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(btn_ren, 40, 44); lv_obj_set_pos(btn_ren, UI_W - 80, 0);
    lv_obj_set_style_bg_opa(btn_ren, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_ren, 0, 0); lv_obj_set_style_border_width(btn_ren, 0, 0);
    lv_obj_t* ri = lv_label_create(btn_ren);
    lv_label_set_text(ri, ICO_EDIT); lv_obj_set_style_text_font(ri, FONT_ICON, 0);
    lv_obj_set_style_text_color(ri, C_TEXT, 0); lv_obj_center(ri);
    _press_fb(btn_ren);
    lv_obj_add_event_cb(btn_ren, [](lv_event_t* e){
        lv_obj_t* overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
        lv_obj_set_style_border_width(overlay, 0, 0); lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

        // ✓ confirmar — izquierda
        lv_obj_t* btn_ok = lv_btn_create(overlay);
        _press_fb(btn_ok);
        lv_obj_set_size(btn_ok, 44, 44); lv_obj_set_pos(btn_ok, 0, 0);
        lv_obj_set_style_bg_opa(btn_ok, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(btn_ok, 0, 0); lv_obj_set_style_border_width(btn_ok, 0, 0);
        lv_obj_t* ok_ico = lv_label_create(btn_ok);
        lv_label_set_text(ok_ico, ICO_CHECK); lv_obj_set_style_text_font(ok_ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(ok_ico, C_ACCENT, 0); lv_obj_center(ok_ico);

        lv_obj_t* title = lv_label_create(overlay);
        lv_label_set_text(title, t(K_RENOMBRAR));
        lv_obj_set_style_text_color(title, C_TEXT, 0);
        lv_obj_set_style_text_font(title, FONT_HDR, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

        // ✗ cancelar — derecha
        lv_obj_t* btn_x = lv_btn_create(overlay);
        _press_fb(btn_x);
        lv_obj_set_size(btn_x, 44, 44); lv_obj_set_pos(btn_x, UI_W - 44, 0);
        lv_obj_set_style_bg_opa(btn_x, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(btn_x, 0, 0); lv_obj_set_style_border_width(btn_x, 0, 0);
        lv_obj_t* x_ico = lv_label_create(btn_x);
        lv_label_set_text(x_ico, ICO_CLOSE); lv_obj_set_style_text_font(x_ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(x_ico, C_MUTED, 0); lv_obj_center(x_ico);

        lv_obj_t* ta = lv_textarea_create(overlay);
        lv_obj_set_size(ta, UI_W - 24, 44); lv_obj_set_pos(ta, 12, 54);
        lv_textarea_set_one_line(ta, true); lv_textarea_set_max_length(ta, VAULT_CAT_LEN - 1);
        lv_textarea_set_text(ta, _cur_cat_name);
        lv_obj_set_style_bg_color(ta, C_CARD, 0); lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(ta, C_ACCENT, 0);
        lv_obj_set_style_border_width(ta, 2, 0); lv_obj_set_style_radius(ta, 10, 0);
        lv_obj_set_style_text_color(ta, C_TEXT, 0); lv_obj_set_style_text_font(ta, FONT_MD, 0);
        lv_obj_set_style_pad_ver(ta, 12, 0); lv_obj_set_style_pad_hor(ta, 12, 0);
        lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);

        struct RenCtx { lv_obj_t* ta; lv_obj_t* overlay; };
        static RenCtx rctx;
        rctx = {ta, overlay};

        auto confirm_ren = [](lv_event_t* ev){
            RenCtx* c = (RenCtx*)lv_event_get_user_data(ev);
            if (c->ta && lv_obj_is_valid(c->ta)) {
                const char* new_name = lv_textarea_get_text(c->ta);
                if (new_name && strlen(new_name) > 0) {
                    if (_cat_display_name_exists(new_name, _cur_cat_id)) {
                        _pending_toast = 2; // ya existe
                    } else if (vault_cat_rename(_cur_cat_id, new_name)) {
                        strncpy(_cur_cat_name, new_name, VAULT_CAT_LEN - 1);
                        _pending_toast = 3; // renombrada
                    }
                }
            }
            kb_reset();
            lv_obj_del(c->overlay);
            _show_category_detail(_cur_cat_id, _cur_cat_name);
            _flush_pending_cat_toast();
        };
        lv_obj_add_event_cb(btn_ok, confirm_ren, LV_EVENT_CLICKED, &rctx);

        lv_obj_add_event_cb(btn_x, [](lv_event_t* ev){
            kb_reset();
            lv_obj_del((lv_obj_t*)lv_event_get_user_data(ev));
        }, LV_EVENT_CLICKED, overlay);

        lv_obj_t* kb = kb_create(overlay, ta, []{
            lv_obj_t* ta_ref = kb_get_ta();
            if (ta_ref && lv_obj_is_valid(ta_ref)) {
                const char* new_name = lv_textarea_get_text(ta_ref);
                if (new_name && strlen(new_name) > 0) {
                    if (_cat_display_name_exists(new_name, _cur_cat_id)) {
                        _pending_toast = 2;
                    } else if (vault_cat_rename(_cur_cat_id, new_name)) {
                        strncpy(_cur_cat_name, new_name, VAULT_CAT_LEN - 1);
                        _pending_toast = 3;
                    }
                }
            }
            kb_reset();
            _show_category_detail(_cur_cat_id, _cur_cat_name);
            _flush_pending_cat_toast();
        });
        lv_obj_set_pos(kb, 0, UI_H - CKB_H);
        kb_show();
        kb_set_ta(ta);
    }, LV_EVENT_CLICKED, nullptr);

    // Botón eliminar categoría — solo si no es t(K_SIN_CATEGORIA)
    lv_obj_t* btn_del = lv_btn_create(hdr);
    if (strcmp(cat_id, CAT_NONE) == 0) lv_obj_add_flag(btn_del, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(btn_del, 40, 44); lv_obj_set_pos(btn_del, UI_W - 40, 0);
    lv_obj_set_style_bg_opa(btn_del, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_del, 0, 0); lv_obj_set_style_border_width(btn_del, 0, 0);
    lv_obj_t* di = lv_label_create(btn_del);
    lv_label_set_text(di, ICO_DELETE); lv_obj_set_style_text_font(di, FONT_ICON, 0);
    lv_obj_set_style_text_color(di, C_DANGER, 0); lv_obj_center(di);
    _press_fb(btn_del);
    lv_obj_add_event_cb(btn_del, [](lv_event_t* e){
        lv_obj_t* overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
        _fade_overlay(overlay, LV_OPA_60);
        lv_obj_set_style_border_width(overlay, 0, 0); lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* box = lv_obj_create(overlay);
        lv_obj_set_size(box, UI_W - 48, 130); lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(box, C_CARD, 0); lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(box, 0, 0); lv_obj_set_style_radius(box, 12, 0);
        lv_obj_set_style_pad_all(box, 14, 0); lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(box, [](lv_event_t* ev){ lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* msg = lv_label_create(box);
        lv_label_set_text(msg, t(K_CONFIRMA_ELIMINAR_CATEGORIA));
        lv_obj_set_style_text_color(msg, C_TEXT, 0); lv_obj_set_style_text_font(msg, FONT_MD, 0);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 0);
        int bw = (UI_W - 48 - 32 - 8) / 2;
        // Cancelar — izquierda
        lv_obj_t* no = lv_btn_create(box);
        _press_fb(no);
        lv_obj_set_size(no, bw, 36); lv_obj_set_pos(no, 0, 72);
        lv_obj_set_style_bg_color(no, C_BORDER, 0); lv_obj_set_style_radius(no, 8, 0);
        lv_obj_set_style_shadow_width(no, 0, 0); lv_obj_set_style_border_width(no, 1, 0);
        lv_obj_set_style_border_color(no, C_MUTED, 0);
        lv_obj_t* nol = lv_label_create(no); lv_label_set_text(nol, t(K_CANCELAR));
        lv_obj_set_style_text_color(nol, C_TEXT, 0);
        lv_obj_set_style_text_font(nol, FONT_SM, 0); lv_obj_center(nol);
        lv_obj_add_event_cb(no, [](lv_event_t* e){
            lv_obj_del(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e))));
        }, LV_EVENT_CLICKED, nullptr);
        // Eliminar — derecha
        lv_obj_t* ok = lv_btn_create(box);
        _press_fb(ok);
        lv_obj_set_size(ok, bw, 36); lv_obj_set_pos(ok, bw + 8, 72);
        lv_obj_set_style_bg_color(ok, C_DANGER, 0); lv_obj_set_style_radius(ok, 8, 0);
        lv_obj_set_style_shadow_width(ok, 0, 0); lv_obj_set_style_border_width(ok, 0, 0);
        lv_obj_t* okl = lv_label_create(ok); lv_label_set_text(okl, t(K_ELIMINAR));
        lv_obj_set_style_text_color(okl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(okl, FONT_SM, 0); lv_obj_center(okl);
        lv_obj_add_event_cb(ok, [](lv_event_t* e){ vault_cat_delete(_cur_cat_id); _show_categories(true); lv_async_call([](void*){ toast_del(t(K_TOAST_CARPETA_ELIMINADA)); }, nullptr); }, LV_EVENT_CLICKED, nullptr);
    }, LV_EVENT_CLICKED, nullptr);

    _divider(scr, 44);

    // ── Lista de entradas ─────────────────────────────────────
    lv_obj_t* list = lv_obj_create(scr);
    lv_obj_set_size(list, UI_W, UI_H - 44); lv_obj_set_pos(list, 0, 44);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 8, 0); lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    if (count == 0) {
        lv_obj_t* empty = lv_label_create(scr);
        lv_label_set_text(empty, t(K_SIN_ENTRADAS_CATEGORIA));
        lv_obj_set_style_text_color(empty, C_MUTED, 0);
        lv_obj_set_style_text_font(empty, FONT_SM, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);
    } else {
        for (int i = 0; i < count; i++) {
            lv_obj_t* row = lv_obj_create(list);
            lv_obj_set_size(row, UI_W - 16, 46);
            lv_obj_set_style_bg_color(row, C_CARD, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_set_style_radius(row, 12, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            _press_fb(row);

            lv_obj_t* nl = lv_label_create(row);
            lv_label_set_text(nl, names[i]);
            lv_obj_set_style_text_color(nl, C_TEXT, 0);
            lv_obj_set_style_text_font(nl, FONT_MD, 0);
            lv_label_set_long_mode(nl, LV_LABEL_LONG_DOT);
            lv_obj_set_width(nl, UI_W - 64);
            lv_obj_set_pos(nl, 12, 6);

            uint8_t ty = vault_get_type(names[i]);
            lv_obj_t* sl = lv_label_create(row);
            lv_label_set_text(sl, ty == VAULT_TYPE_WEB ? t(K_CUENTA) : t(K_OBJETO));
            lv_obj_set_style_text_color(sl, C_MUTED, 0);
            lv_obj_set_style_text_font(sl, FONT_SM, 0);
            lv_obj_set_pos(sl, 12, 26);

            if (vault_get_favorite(names[i])) {
                lv_obj_t* star = lv_label_create(row);
                lv_label_set_text(star, ICO_STAR);
                lv_obj_set_style_text_font(star, FONT_ICON, 0);
                lv_obj_set_style_text_color(star, lv_color_hex(0xFFCC00), 0);
                lv_obj_set_pos(star, UI_W - 52, 14);
                lv_obj_clear_flag(star, LV_OBJ_FLAG_CLICKABLE);
            }

            lv_obj_t* ar = lv_label_create(row);
            lv_label_set_text(ar, ICO_CHEVRON_RIGHT);
            lv_obj_set_style_text_font(ar, FONT_ICON, 0);
            lv_obj_set_style_text_color(ar, C_MUTED, 0);
            lv_obj_set_pos(ar, UI_W - 36, 14);
            lv_obj_clear_flag(ar, LV_OBJ_FLAG_CLICKABLE);

            char* np = names[i];
            lv_obj_add_event_cb(row, [](lv_event_t* e){
                char* n = (char*)lv_event_get_user_data(e);
                strncpy(_selected_name, n, VAULT_FIELD_LEN-1);
                _show_detail(_selected_name);
            }, LV_EVENT_CLICKED, np);
        }
    }
    _load_screen(scr);
}

// ── PANTALLA: Carpetas ──────────────────────────────────────
static void _show_categories(bool back) {
    _current_screen = SCREEN_CATEGORIES; _kb_active = false;
    lv_obj_t* scr = _make_screen();

    // ── Header ────────────────────────────────────────────────
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
    lv_obj_add_event_cb(bb, [](lv_event_t* e){ _show_list(); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* tt = lv_label_create(hdr);
    lv_label_set_text(tt, t(K_CARPETAS));
    lv_obj_set_style_text_color(tt, C_TEXT, 0);
    lv_obj_set_style_text_font(tt, FONT_MD, 0);
    lv_obj_align(tt, LV_ALIGN_CENTER, 0, 0);

    // Botón + nueva categoría
    lv_obj_t* btn_add = lv_btn_create(hdr);
    lv_obj_set_size(btn_add, 40, 44); lv_obj_set_pos(btn_add, UI_W - 40, 0);
    lv_obj_set_style_bg_opa(btn_add, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_add, 0, 0); lv_obj_set_style_border_width(btn_add, 0, 0);
    lv_obj_t* ai = lv_label_create(btn_add);
    lv_label_set_text(ai, ICO_ADD); lv_obj_set_style_text_font(ai, FONT_ICON, 0);
    lv_obj_set_style_text_color(ai, C_ACCENT, 0); lv_obj_center(ai);
    _press_fb(btn_add);
    lv_obj_add_event_cb(btn_add, [](lv_event_t* e){
        // Overlay sobre la pantalla actual
        lv_obj_t* overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(overlay, UI_W, UI_H); lv_obj_set_pos(overlay, 0, 0);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
        lv_obj_set_style_border_width(overlay, 0, 0); lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

        // ✓ confirmar — izquierda
        lv_obj_t* btn_ok = lv_btn_create(overlay);
        _press_fb(btn_ok);
        lv_obj_set_size(btn_ok, 44, 44); lv_obj_set_pos(btn_ok, 0, 0);
        lv_obj_set_style_bg_opa(btn_ok, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(btn_ok, 0, 0); lv_obj_set_style_border_width(btn_ok, 0, 0);
        lv_obj_t* ok_ico = lv_label_create(btn_ok);
        lv_label_set_text(ok_ico, ICO_CHECK); lv_obj_set_style_text_font(ok_ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(ok_ico, C_ACCENT, 0); lv_obj_center(ok_ico);

        // Título centrado
        lv_obj_t* title = lv_label_create(overlay);
        lv_label_set_text(title, t(K_NUEVA_CATEGORIA));
        lv_obj_set_style_text_color(title, C_TEXT, 0);
        lv_obj_set_style_text_font(title, FONT_HDR, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

        // ✗ cancelar — derecha
        lv_obj_t* btn_x = lv_btn_create(overlay);
        _press_fb(btn_x);
        lv_obj_set_size(btn_x, 44, 44); lv_obj_set_pos(btn_x, UI_W - 44, 0);
        lv_obj_set_style_bg_opa(btn_x, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(btn_x, 0, 0); lv_obj_set_style_border_width(btn_x, 0, 0);
        lv_obj_t* x_ico = lv_label_create(btn_x);
        lv_label_set_text(x_ico, ICO_CLOSE); lv_obj_set_style_text_font(x_ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(x_ico, C_MUTED, 0); lv_obj_center(x_ico);

        // Textarea
        lv_obj_t* ta = lv_textarea_create(overlay);
        lv_obj_set_size(ta, UI_W - 24, 44); lv_obj_set_pos(ta, 12, 54);
        lv_textarea_set_one_line(ta, true); lv_textarea_set_max_length(ta, 32);
        lv_textarea_set_placeholder_text(ta, t(K_EJ_REDES_SOCIALES));
        lv_obj_set_style_bg_color(ta, C_CARD, 0); lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(ta, C_ACCENT, 0);
        lv_obj_set_style_border_width(ta, 2, 0); lv_obj_set_style_radius(ta, 10, 0);
        lv_obj_set_style_text_color(ta, C_TEXT, 0); lv_obj_set_style_text_font(ta, FONT_MD, 0);
        lv_obj_set_style_pad_ver(ta, 12, 0); lv_obj_set_style_pad_hor(ta, 12, 0);
        lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);

        // Callbacks confirmar
        struct CatCtx { lv_obj_t* ta; lv_obj_t* overlay; };
        static CatCtx cctx;
        cctx = {ta, overlay};

        auto confirm = [](lv_event_t* e){
            CatCtx* c = (CatCtx*)lv_event_get_user_data(e);
            if (c->ta && lv_obj_is_valid(c->ta)) {
                const char* name = lv_textarea_get_text(c->ta);
                if (strlen(name) > 0) {
                    if (_cat_display_name_exists(name)) _pending_toast = 2;       // error: ya existe
                    else if (vault_cat_create(name))    _pending_toast = 1;       // ok: creada
                }
            }
            kb_reset();
            lv_obj_del(c->overlay);
            _show_categories(true);
            _flush_pending_cat_toast();
        };
        lv_obj_add_event_cb(btn_ok, confirm, LV_EVENT_CLICKED, &cctx);

        // Cancelar
        lv_obj_add_event_cb(btn_x, [](lv_event_t* e){
            kb_reset();
            lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
        }, LV_EVENT_CLICKED, overlay);

        // Custom keyboard — OK también confirma
        lv_obj_t* kb = kb_create(overlay, ta, []{
            lv_obj_t* ta = kb_get_ta();
            if (ta && lv_obj_is_valid(ta)) {
                const char* name = lv_textarea_get_text(ta);
                if (strlen(name) > 0) {
                    if (_cat_display_name_exists(name)) _pending_toast = 2;
                    else if (vault_cat_create(name))    _pending_toast = 1;
                }
            }
            kb_reset();
            _show_categories(true);
            _flush_pending_cat_toast();
        });
        lv_obj_set_pos(kb, 0, UI_H - CKB_H);
        kb_show();
        kb_set_ta(ta);
    }, LV_EVENT_CLICKED, nullptr);

    _divider(scr, 44);

    // ── Lista de categorías ───────────────────────────────────
    lv_obj_t* list = lv_obj_create(scr);
    lv_obj_set_size(list, UI_W, UI_H - 44); lv_obj_set_pos(list, 0, 44);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 8, 0); lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    VaultCategory cats[VAULT_MAX_CATS + 6];
    int cat_count = 0;
    vault_cat_list(cats, &cat_count, VAULT_MAX_CATS + 6);

    struct CatNavData { char id[VAULT_CAT_LEN]; char name[VAULT_CAT_LEN]; };
    static CatNavData _cat_nav_opts[VAULT_MAX_CATS + 6];
    static int _cat_nav_count = 0;
    _cat_nav_count = 0;

    // Contar entradas sin categoría
    int none_count = vault_count(CAT_NONE);

    if (cat_count == 0 && none_count == 0) {
        lv_obj_t* empty = lv_label_create(scr);
        lv_label_set_text(empty, t(K_SIN_CATEGORIAS_TOCA));
        lv_obj_set_style_text_color(empty, C_MUTED, 0);
        lv_obj_set_style_text_font(empty, FONT_SM, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);
    }

    for (int i = 0; i < cat_count; i++) {
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, UI_W - 16, 46);
        lv_obj_set_style_bg_color(row, C_CARD, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        _press_fb(row);

        lv_obj_t* ico = lv_label_create(row);
        lv_label_set_text(ico, ICO_FOLDER);
        lv_obj_set_style_text_font(ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico, C_ACCENT, 0);
        lv_obj_set_pos(ico, 12, 14);
        lv_obj_clear_flag(ico, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* nl = lv_label_create(row);
        lv_label_set_text(nl, _cat_display_name(cats[i]));
        lv_obj_set_style_text_color(nl, C_TEXT, 0);
        lv_obj_set_style_text_font(nl, FONT_MD, 0);
        lv_label_set_long_mode(nl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nl, UI_W - 80);
        lv_obj_set_pos(nl, 40, 14);
        lv_obj_clear_flag(nl, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* ar = lv_label_create(row);
        lv_label_set_text(ar, ICO_CHEVRON_RIGHT);
        lv_obj_set_style_text_font(ar, FONT_ICON, 0);
        lv_obj_set_style_text_color(ar, C_MUTED, 0);
        lv_obj_set_pos(ar, UI_W - 36, 14);
        lv_obj_clear_flag(ar, LV_OBJ_FLAG_CLICKABLE);

        int idx = _cat_nav_count;
        strncpy(_cat_nav_opts[idx].id,   cats[i].id,   VAULT_CAT_LEN-1);
        strncpy(_cat_nav_opts[idx].name, _cat_display_name(cats[i]), VAULT_CAT_LEN-1);
        _cat_nav_count++;

        lv_obj_add_event_cb(row, [](lv_event_t* e){
            int i2 = (int)(intptr_t)lv_event_get_user_data(e);
            _show_category_detail(_cat_nav_opts[i2].id, _cat_nav_opts[i2].name);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
    }

    // Fila t(K_SIN_CATEGORIA) — siempre al final si hay entradas sin categoría
    if (none_count > 0) {
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, UI_W - 16, 46);
        lv_obj_set_style_bg_color(row, C_CARD, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        _press_fb(row);

        lv_obj_t* ico = lv_label_create(row);
        lv_label_set_text(ico, ICO_FOLDER);
        lv_obj_set_style_text_font(ico, FONT_ICON, 0);
        lv_obj_set_style_text_color(ico, C_MUTED, 0);
        lv_obj_set_pos(ico, 12, 14);
        lv_obj_clear_flag(ico, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* nl = lv_label_create(row);
        lv_label_set_text(nl, t(K_SIN_CATEGORIA));
        lv_obj_set_style_text_color(nl, C_TEXT, 0);
        lv_obj_set_style_text_font(nl, FONT_MD, 0);
        lv_label_set_long_mode(nl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nl, UI_W - 80);
        lv_obj_set_pos(nl, 40, 14);
        lv_obj_clear_flag(nl, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* ar = lv_label_create(row);
        lv_label_set_text(ar, ICO_CHEVRON_RIGHT);
        lv_obj_set_style_text_font(ar, FONT_ICON, 0);
        lv_obj_set_style_text_color(ar, C_MUTED, 0);
        lv_obj_set_pos(ar, UI_W - 36, 14);
        lv_obj_clear_flag(ar, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_add_event_cb(row, [](lv_event_t* e){
            _show_category_detail(CAT_NONE, t(K_SIN_CATEGORIA));
        }, LV_EVENT_CLICKED, nullptr);
    }

    if (back) _load_screen_back(scr); else _load_screen(scr);
}


static void _show_search() {
    kb_reset();
    _current_screen = SCREEN_SEARCH;
    ui_statusbar_set_visible(false);
    memset(_search_buf, 0, sizeof(_search_buf));
    // Asegurar que las entradas están cargadas y ordenadas
    vault_list(_entry_names, &_entry_count, VAULT_MAX_ENTRIES);
    _sort_entries();

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

    // Botón volver
    lv_obj_t* btn_back = lv_btn_create(hdr);
    lv_obj_set_size(btn_back, 40, 44);
    lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_t* back_ico = lv_label_create(btn_back);
    lv_label_set_text(back_ico, ICO_ARROW_BACK);
    lv_obj_set_style_text_font(back_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(back_ico, C_TEXT, 0);
    lv_obj_center(back_ico);
    lv_obj_add_event_cb(btn_back, [](lv_event_t* e){
        memset(_search_buf, 0, sizeof(_search_buf));
        kb_reset();
        _show_list();
    }, LV_EVENT_CLICKED, nullptr);

    // Campo de búsqueda — ancho completo menos volver y X
    lv_obj_t* ta = lv_textarea_create(hdr);
    lv_obj_set_size(ta, UI_W - 84, 32);
    lv_obj_set_pos(ta, 44, 6);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, t(K_BUSCAR_PLACEHOLDER));
    lv_obj_set_style_bg_color(ta, C_BORDER, 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ta, 0, 0);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_text_color(ta, C_TEXT, 0);
    lv_obj_set_style_text_font(ta, FONT_SM, 0);
    lv_obj_set_style_pad_hor(ta, 10, 0);
    lv_obj_set_style_pad_top(ta, 8, 0);
    lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);
    _kb_apply_cursor(ta);

    // Botón X — borrar búsqueda, oculto hasta que haya texto
    lv_obj_t* btn_x = lv_btn_create(hdr);
    _press_fb(btn_x);
    lv_obj_set_size(btn_x, 40, 44);
    lv_obj_set_pos(btn_x, UI_W - 40, 0);
    lv_obj_set_style_bg_opa(btn_x, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_x, 0, 0);
    lv_obj_set_style_border_width(btn_x, 0, 0);
    lv_obj_t* x_ico = lv_label_create(btn_x);
    lv_label_set_text(x_ico, ICO_CLOSE);
    lv_obj_set_style_text_font(x_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(x_ico, C_MUTED, 0);
    lv_obj_center(x_ico);
    lv_obj_add_flag(btn_x, LV_OBJ_FLAG_HIDDEN); // oculto por defecto
    lv_obj_add_event_cb(btn_x, [](lv_event_t* e){
        // Borrar texto del textarea
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* hdr_obj = lv_obj_get_parent(btn);
        lv_obj_t* ta_obj = lv_obj_get_child(hdr_obj, 1); // ta es el segundo hijo del hdr
        lv_textarea_set_text(ta_obj, "");
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, nullptr);

    _divider(scr, 44);

    // ── Lista de resultados — altura hasta el fondo ───────────
    lv_obj_t* list = lv_obj_create(scr);
    lv_obj_set_size(list, UI_W, UI_H - 50);
    lv_obj_set_pos(list, 0, 50);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 8, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Mensaje inicial
    lv_obj_t* hint = lv_label_create(list);
    lv_label_set_text(hint, t(K_ESCRIBE_PARA_BUSCAR));
    lv_obj_set_style_text_color(hint, C_MUTED, 0);
    lv_obj_set_style_text_font(hint, FONT_SM, 0);
    lv_obj_center(hint);

    // VALUE_CHANGED — actualizar lista y mostrar/ocultar btn_x
    lv_obj_add_event_cb(ta, [](lv_event_t* e){
        lv_obj_t* ta_obj  = (lv_obj_t*)lv_event_get_target(e);
        _kb_apply_cursor(ta_obj);
        lv_obj_t* scr_obj = lv_scr_act();
        strncpy(_search_buf, lv_textarea_get_text(ta_obj), VAULT_FIELD_LEN-1);

        // btn_x: hijo 2 del hdr (hijo 0 del scr)
        lv_obj_t* hdr_obj = lv_obj_get_child(scr_obj, 0);
        lv_obj_t* btnx    = lv_obj_get_child(hdr_obj, 2);
        if (strlen(_search_buf) > 0)
            lv_obj_clear_flag(btnx, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(btnx, LV_OBJ_FLAG_HIDDEN);

        // list: hijo 2 del scr (0=hdr, 1=divider, 2=list)
        lv_obj_t* list_obj = lv_obj_get_child(scr_obj, 2);
        if (!list_obj) return;
        lv_obj_clean(list_obj);

        if (strlen(_search_buf) == 0) {
            lv_obj_t* hint2 = lv_label_create(list_obj);
            lv_label_set_text(hint2, t(K_ESCRIBE_PARA_BUSCAR));
            lv_obj_set_style_text_color(hint2, C_MUTED, 0);
            lv_obj_set_style_text_font(hint2, FONT_SM, 0);
            lv_obj_center(hint2);
            return;
        }

        _apply_filter();
        if (_filtered_count == 0) {
            lv_obj_t* empty = lv_label_create(list_obj);
            lv_label_set_text(empty, t(K_SIN_RESULTADOS));
            lv_obj_set_style_text_color(empty, C_MUTED, 0);
            lv_obj_set_style_text_font(empty, FONT_SM, 0);
            lv_obj_center(empty);
            return;
        }

        // Limitar a 20 resultados para no agotar el pool de objetos LVGL
        int max_results = _filtered_count > 20 ? 20 : _filtered_count;
        for (int i = 0; i < max_results; i++) {
            int idx = _filtered[i];
            VaultEntry e2 = vault_load(_entry_names[idx]);
            lv_obj_t* row = lv_obj_create(list_obj);
            lv_obj_set_size(row, UI_W - 16, 54);
            lv_obj_set_style_bg_color(row, C_CARD, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 8, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* name_lbl = lv_label_create(row);
            lv_label_set_text(name_lbl, _entry_names[idx]);
            lv_obj_set_style_text_font(name_lbl, FONT_MD, 0);
            lv_obj_set_style_text_color(name_lbl, C_TEXT, 0);
            lv_obj_set_pos(name_lbl, 12, 8);

            if (e2.valid && strlen(e2.username) > 0) {
                lv_obj_t* user_lbl = lv_label_create(row);
                lv_label_set_text(user_lbl, e2.username);
                lv_obj_set_style_text_font(user_lbl, FONT_SM, 0);
                lv_obj_set_style_text_color(user_lbl, C_MUTED, 0);
                lv_obj_set_pos(user_lbl, 12, 28);
            }

            lv_obj_add_event_cb(row, [](lv_event_t* ev){
                lv_obj_t* r  = (lv_obj_t*)lv_event_get_target(ev);
                lv_obj_t* nl = lv_obj_get_child(r, 0);
                strncpy(_selected_name, lv_label_get_text(nl), VAULT_FIELD_LEN-1);
                memset(_search_buf, 0, sizeof(_search_buf));
                kb_reset();
                _show_detail(_selected_name);
            }, LV_EVENT_CLICKED, nullptr);
        }
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // CLICKED en ta — crear overlay en lv_layer_top para cerrar teclado
    lv_obj_add_event_cb(ta, [](lv_event_t* e){
        // Crear overlay transparente sobre la lista en lv_layer_top
        lv_obj_t* top = lv_layer_top();
        // Evitar crear múltiples overlays
        if (lv_obj_get_child_cnt(top) > 0) return;
        lv_obj_t* ov = lv_obj_create(top);
        lv_obj_set_size(ov, UI_W, UI_H - 50 - CKB_H);
        lv_obj_set_pos(ov, 0, 50);
        lv_obj_set_style_bg_opa(ov, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ov, 0, 0);
        lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(ov, [](lv_event_t* ev){
            lv_obj_del((lv_obj_t*)lv_event_get_target(ev));
            kb_hide();
        }, LV_EVENT_CLICKED, nullptr);
    }, LV_EVENT_CLICKED, nullptr);

    // ── Teclado — lazy creation al primer toque del ta ────────
    _search_kb = nullptr;
    lv_obj_add_event_cb(ta, [](lv_event_t* e){
        lv_obj_t* ta_obj = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* scr_obj = lv_obj_get_parent(ta_obj)->parent ? lv_scr_act() : lv_scr_act();
        if (!_search_kb) {
            _search_kb = kb_create(lv_scr_act(), ta_obj, [](){
                memset(_search_buf, 0, sizeof(_search_buf));
                // Limpiar overlay si existe
                lv_obj_t* top = lv_layer_top();
                if (lv_obj_get_child_cnt(top) > 0)
                    lv_obj_del(lv_obj_get_child(top, 0));
                kb_reset();
                _show_list();
            });
            lv_obj_set_pos(_search_kb, 0, UI_H - CKB_H);
        }
        kb_show();
    }, LV_EVENT_FOCUSED, nullptr);

    _load_screen_instant(scr);
}
