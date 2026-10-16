#pragma once
// =============================================================
//  ui_main.h — UI fiel al diseño de referencia
//  240x320 vertical, LVGL 8.x
//
//  Diseño:
//  - Sin cajas/tarjetas en filas — separadores finos
//  - Campos con línea inferior, sin borde completo
//  - Tipografía limpia, sans-serif
//  - "VAULT" pequeño uppercase en header
//  - FAB + circular centrado en barra inferior
// =============================================================

#include <lvgl.h>
#include "vault.h"
#include "vibro_manager.h"
#include "i18n.h"

// Traduce el marcador de una carpeta de fábrica sin renombrar
// (DEFAULT_1.._DEFAULT_5) al nombre del idioma activo. Si ya fue
// renombrada por el usuario, default_idx es 0 y se ignora.
static const char* _cat_display_name(const VaultCategory& cat) {
    static const int keys[5] = { K_CAT_FINANZAS, K_CAT_TRABAJO, K_CAT_STREAMING, K_CAT_HOBBY, K_CAT_PERSONAL };
    if (cat.default_idx >= 1 && cat.default_idx <= 5) return t(keys[cat.default_idx - 1]);
    return cat.name;
}
// Misma traducción pero a partir del cat_id (para vault_cat_name(), que
// solo devuelve el String crudo — puede ser un marcador o un nombre real).
static const char* _cat_display_name_raw(const char* raw_name) {
    static const int keys[5] = { K_CAT_FINANZAS, K_CAT_TRABAJO, K_CAT_STREAMING, K_CAT_HOBBY, K_CAT_PERSONAL };
    if (raw_name && strncmp(raw_name, "DEFAULT_", 8) == 0) {
        int n = atoi(raw_name + 8);
        if (n >= 1 && n <= 5) return t(keys[n - 1]);
    }
    return raw_name;
}

// ¿Existe ya una carpeta cuyo NOMBRE MOSTRADO (traducido) coincide con
// 'display_name' (ignorando mayúsculas)? Compara contra el nombre visible,
// no el crudo, para que "Finance" (traducción de DEFAULT_1) cuente como
// duplicado de una carpeta manual llamada "Finance". Se puede excluir un
// cat_id (el que se está renombrando) para no compararlo consigo mismo.
static bool _cat_display_name_exists(const char* display_name, const char* exclude_id = nullptr) {
    if (!display_name || strlen(display_name) == 0) return false;
    VaultCategory cats[VAULT_MAX_CATS + 6];
    int count = 0;
    vault_cat_list(cats, &count, VAULT_MAX_CATS + 6);
    for (int i = 0; i < count; i++) {
        if (!cats[i].valid) continue;
        if (exclude_id && strcmp(cats[i].id, exclude_id) == 0) continue;
        const char* shown = _cat_display_name(cats[i]);
        if (shown && strcasecmp(shown, display_name) == 0) return true;
    }
    return false;
}


#define UI_W   240
#define UI_H   320

// Pantalla activa
static lv_obj_t* _active_scr = nullptr;

// Aplica fade de entrada al overlay (de transparente a opacidad objetivo)
// Feedback táctil — aplica a cualquier objeto clickeable
static void _press_fb(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, C_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_add_event_cb(obj, [](lv_event_t* e){ vibro_pulse(); }, LV_EVENT_CLICKED, nullptr);
}

static void _fade_overlay(lv_obj_t* overlay, lv_opa_t target_opa) {
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, overlay);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_bg_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
    });
    lv_anim_set_values(&a, LV_OPA_TRANSP, target_opa);
    lv_anim_set_time(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// Tipos de pantalla — movido aquí (antes de _load_screen*) porque ahora
// esos helpers necesitan saber qué pantalla es para la barra de estado.
typedef enum {
    SCREEN_LIST = 0,
    SCREEN_DETAIL,
    SCREEN_FORM_WEB,
    SCREEN_FORM_SECRET,
    SCREEN_SETTINGS,
    SCREEN_CHANGE_PIN,
    SCREEN_DECOY_SETUP,
    SCREEN_CATEGORIES,
    SCREEN_CAT_DETAIL,
    SCREEN_FAVORITES,
    SCREEN_INFO,
    SCREEN_PRIVACY,
    SCREEN_ABOUT,
    SCREEN_IMPORT_EXPORT,
    SCREEN_SEARCH,
    SCREEN_NONE
} ScreenType;

static ScreenType _current_screen = SCREEN_LIST;
void ui_statusbar_set_visible(bool visible); // forward decl — definida en ui_statusbar.h
void ui_statusbar_apply_theme(int t); // forward decl — definida en ui_statusbar.h

// La barra de estado SOLO se muestra en la pantalla de lista — se evalúa
// automáticamente cada vez que cualquier pantalla se carga por estos 3
// helpers, así no hay que tocar cada _show_X() individualmente.
static void _sb_sync_visibility() {
    ui_statusbar_set_visible(_current_screen == SCREEN_LIST);
}

static void _load_screen(lv_obj_t* new_scr) {
    _sb_sync_visibility();
    lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, true);
    _active_scr = new_scr;
}

static void _load_screen_back(lv_obj_t* new_scr) {
    _sb_sync_visibility();
    lv_obj_t* cur = lv_scr_act();
    if (!cur || cur == new_scr) {
        lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_NONE, 0, 50, true);
    } else {
        lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, true);
    }
    _active_scr = new_scr;
}

static void _load_screen_instant(lv_obj_t* new_scr) {
    _sb_sync_visibility();
    lv_obj_t* old = lv_scr_act();
    lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    // Liberación asíncrona de la pantalla anterior
    if (old && old != new_scr && lv_obj_is_valid(old)) lv_obj_del_async(old);
    _active_scr = new_scr;
}

// ── Colores por tema ──────────────────────────────────────────
#include "colors.h"
#include "fonts.h"
#include "ui_toast.h"   // sistema de toasts (usa UI_W, FONT_SM; disponible para todas las pantallas)
// Helper: aplica estilo de cursor visible a un textarea
static void _style_ta(lv_obj_t* ta) {
    lv_obj_set_style_bg_color(ta, C_ACCENT, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ta, 0, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_anim_time(ta, 500, 0);
}

// kb_manager.h provee: kb_create, kb_reset, kb_show, kb_hide, kb_set_ta, kb_get_ta
#define _kb_create  kb_create
#define _kb_reset   kb_reset
#define _kb_show    kb_show
#define _kb_hide    kb_hide
#define _kb_set_ta  kb_set_ta
#define _kb_get_ta  kb_get_ta

// ── Wrappers globales accesibles desde lambdas ───────────────
static void _gen_password(char* buf, int len, bool upper, bool nums, bool syms, bool ambig) {
    vault_generate_password(buf, len, upper, nums, syms, ambig);
}
static VaultGenConfig _load_gen_config() {
    return vault_get_gen_config();
}
static void _save_gen_config(int len, bool upper, bool nums, bool syms, bool ambig) {
    vault_set_gen_config(len, upper, nums, syms, ambig);
}
static int _current_theme = 0;

// Forward declarations — definidas más abajo (barra de estado)
#define STATUSBAR_H 18

void ui_statusbar_init();
void ui_statusbar_set_visible(bool visible);
void ui_statusbar_tick(bool force = false);

static void _apply_theme(int t) {
    _current_theme = t;
    int base = (t == THEME_PERSONALIZADO) ? vault_get_custom_theme().base : t;
    if (base == 0) {  // oscuro
        C_BG       = lv_color_hex(0x1C1C1E);
        C_TEXT     = lv_color_hex(0xFFFFFF);
        C_MUTED    = lv_color_hex(0x8E8E93);
        C_BORDER   = lv_color_hex(0x2C2C2E);
        C_CARD     = lv_color_hex(0x2C2C2E);
        C_BTN_BG   = lv_color_hex(0xE7EBF1); // = fondo del modo claro
        C_BTN_TEXT = lv_color_hex(0x1C1C1E);
    } else {       // claro
        C_BG       = lv_color_hex(0xE7EBF1);
        C_TEXT     = lv_color_hex(0x000000);
        C_MUTED    = lv_color_hex(0x666666);
        C_BORDER   = lv_color_hex(0xD4D9E0);
        C_CARD     = lv_color_hex(0xFFFFFF);
        C_BTN_BG   = lv_color_hex(0x1C1C1E);
        C_BTN_TEXT = lv_color_hex(0xE7EBF1);
    }
    C_DANGER = lv_color_hex(0xEE3333);

    if (t == THEME_PERSONALIZADO) {
        // Fondo/tarjeta con el matiz+intensidad elegidos; brillo según 'base'.
        VaultCustomTheme ct = vault_get_custom_theme();
        C_BG   = hsv_to_lv_color(ct.bg_h,   ct.bg_s,   base == 0 ? 16 : 94);
        C_CARD = hsv_to_lv_color(ct.card_h, ct.card_s, base == 0 ? 24 : 100);
        C_ACCENT = hsv_to_lv_color(ct.acc_h, ct.acc_s, 92);
        C_ACCENT_TEXT = color_is_light(C_ACCENT) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
    } else {
        int _acc_idx = vault_get_accent();
        if (_acc_idx == ACCENT_AUTO) {
            C_ACCENT = C_BTN_BG;       // contraste invertido: oscuro en tema claro, claro en tema oscuro
            C_ACCENT_TEXT = C_BTN_TEXT;
        } else {
            if (_acc_idx < 0 || _acc_idx >= ACCENT_COUNT) _acc_idx = 0;
            C_ACCENT = lv_color_hex(ACCENT_PALETTE[_acc_idx]);
            C_ACCENT_TEXT = lv_color_hex(0xFFFFFF); // fijo, no depende del tema
        }
    }

    ui_statusbar_init();      // por si nunca corrió (no pasa por pin_screen.h en este camino)
    ui_statusbar_apply_theme(t); // refresca color con el tema recién aplicado, no el guardado en NVS (evita desfase de 1 cambio)
    ui_statusbar_tick(true);  // refresca colores aunque el reloj/batería no haya cambiado
}

// Forward declarations USB/BLE HID
void hid_type_user(const char* user);
void hid_type_pass(const char* pass);
void hid_type_secret(const char* secret);
void hid_type_tab();
void hid_type_enter();
void hid_type_string(const char* str);
void kb_type_user(const char* user);
void kb_type_pass(const char* pass);
void kb_type_secret(const char* secret);
void kb_type_tab();
void kb_type_enter();
void kb_type_string(const char* str);
#if KB_MODE_BLE
void ble_type_diagnostic();
#endif

// ── Navegación por gestos ─────────────────────────────────────
// Tipos de pantalla para swipe
static bool _kb_active = false; // teclado activo — deshabilita swipe
static char (*_entry_names)[VAULT_FIELD_LEN] = nullptr; // PSRAM, asignado en ui_main_init()
static VaultListMetaItem* _entry_meta = nullptr; // PSRAM, username/type/favorite por índice (evita abrir archivos al listar)
static int  _entry_count = 0;
static int  _entry_meta_count = 0;

// Busca metadata (username/type/favorite) ya cargada en RAM por nombre,
// evitando abrir el archivo de la entrada solo para renderizar una fila.
static VaultListMetaItem* _find_entry_meta(const char* name) {
    if (!_entry_meta) return nullptr;
    for (int i = 0; i < _entry_meta_count; i++) {
        if (strcmp(_entry_meta[i].name, name) == 0) return &_entry_meta[i];
    }
    return nullptr;
}
static char _selected_name[VAULT_FIELD_LEN];

// Sort y vista
#define SORT_NAME_AZ   0
#define SORT_NAME_ZA   1
#define SORT_DATE_ASC  2
#define SORT_DATE_DESC 3
static int  _sort_mode    = SORT_NAME_AZ;
static bool _view_icons   = false;
static bool _prefs_loaded = false;

// Modo selección múltiple
static bool  _sel_mode    = false;
static bool* _sel_map = nullptr;  // PSRAM, seleccionados por índice filtrado — asignado en _show_list()
static int   _sel_count   = 0;
static lv_obj_t* _sel_counter_lbl = nullptr;  // label del contador
static lv_obj_t* _sel_del_btn     = nullptr;  // botón eliminar
static lv_obj_t* _sel_all_btn     = nullptr;  // botón seleccionar todo
static lv_obj_t* _sel_cancel_btn  = nullptr;  // botón X cancelar
static lv_obj_t* _search_cont     = nullptr;  // contenedor buscador

// Búsqueda
static char _search_buf[VAULT_FIELD_LEN] = {0};
static lv_obj_t* _list_cont = nullptr;
static lv_obj_t* _search_kb = nullptr;

// Buffers formularios
static char _fw_name_buf[VAULT_FIELD_LEN]   = {0};
static char _fw_user_buf[VAULT_FIELD_LEN]   = {0};
static char _fw_pass_buf[VAULT_FIELD_LEN]   = {0};
static char _fw_notes_buf[VAULT_NOTES_LEN]  = {0};
static char _fw_cat_buf[VAULT_CAT_LEN]      = {0};
static char _fs_name_buf[VAULT_FIELD_LEN]   = {0};
static char _fs_secret_buf[VAULT_FIELD_LEN] = {0};
static char _fs_notes_buf[VAULT_NOTES_LEN]  = {0};
static char _fs_cat_buf[VAULT_CAT_LEN]      = {0};
static bool _fw_editing = false;
static bool _fs_editing = false;
static bool _fw_pass_visible = false;
static bool _fs_secret_visible = false;
static bool _fs_note_only = false;      // toggle "Guardar solo nota" (form objeto)
static lv_obj_t* _fw_ta_pass   = nullptr;
static lv_obj_t* _fw_ta_notes  = nullptr;
static lv_obj_t* _fw_ta_name   = nullptr;
static lv_obj_t* _fw_ta_user   = nullptr;
static lv_obj_t* _fs_ta_secret = nullptr;
static lv_obj_t* _fs_ta_notes  = nullptr;
static lv_obj_t* _fw_scr  = nullptr;
static lv_obj_t* _fs_scr  = nullptr;
static lv_obj_t* _fw_kb   = nullptr;
static lv_obj_t* _fw_btn_x = nullptr;
static lv_obj_t* _fs_kb   = nullptr;
static lv_obj_t* _fs_btn_x = nullptr;
static int  _kb_return_form = 0;
static int  _kb_field_idx   = 0;
static bool _kb_is_password = false;

// Helper: crear teclado formulario web lazily
static void _fw_ensure_kb(lv_obj_t* ta) {
    if (!_fw_kb && _fw_scr) {
        Serial.printf("[KB] Creando teclado fw, heap=%d\n", esp_get_free_heap_size());
        _fw_kb = _kb_create(_fw_scr, ta, [](){
            if (_fw_btn_x) lv_obj_add_flag(_fw_btn_x, LV_OBJ_FLAG_HIDDEN);
            _kb_hide();
        });
        lv_obj_set_pos(_fw_kb, 0, UI_H - CKB_H);
        Serial.printf("[KB] Teclado creado, heap=%d\n", esp_get_free_heap_size());
    }
    _kb_show();
    _kb_set_ta(ta);
}

// Helper: crear teclado formulario secreto lazily
static void _fs_ensure_kb(lv_obj_t* ta) {
    if (!_fs_kb && _fs_scr) {
        _fs_kb = _kb_create(_fs_scr, ta, [](){
            if (_fs_btn_x) lv_obj_add_flag(_fs_btn_x, LV_OBJ_FLAG_HIDDEN);
            _kb_hide();
        });
        lv_obj_set_pos(_fs_kb, 0, UI_H - CKB_H);
    }
    _kb_show();
    _kb_set_ta(ta);
}

static void _show_form_web(bool editing, bool instant = false);
static void _show_form_secret(bool editing, bool instant = false);

// Forward declarations
static lv_obj_t* _make_screen();
static lv_obj_t* _divider(lv_obj_t* parent, int y);

// Editor de campo dedicado — evita que el teclado tape los campos
struct FieldEditorCtx {
    char*       buf;         // buffer donde guardar el valor
    int         buf_len;
    bool        password;    // modo contraseña
    const char* label;       // título del campo
    void        (*on_done)();// callback al terminar
};
static FieldEditorCtx _fed_ctx;

static void _show_field_editor(const char* label, char* buf, int buf_len,
                                bool password, void(*on_done)()) {
    _fed_ctx = {buf, buf_len, password, label, on_done};

    lv_obj_t* scr_act = lv_scr_act();

    lv_obj_t* overlay = lv_obj_create(scr_act);
    lv_obj_set_size(overlay, UI_W, UI_H);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, C_BG, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Confirmar
    lv_obj_t* btn_ok = lv_btn_create(overlay);
    _press_fb(btn_ok);
    lv_obj_set_size(btn_ok, 38, 38); lv_obj_set_pos(btn_ok, 4, 8);
    lv_obj_set_style_bg_opa(btn_ok, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_ok, 0, 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_t* ok_ico = lv_label_create(btn_ok);
    lv_label_set_text(ok_ico, ICO_CHECK);
    lv_obj_set_style_text_font(ok_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(ok_ico, C_ACCENT, 0);
    lv_obj_center(ok_ico);

    // Nombre del campo
    lv_obj_t* lbl = lv_label_create(overlay);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_HDR, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 48);

    // Cancelar
    lv_obj_t* btn_cancel = lv_btn_create(overlay);
    _press_fb(btn_cancel);
    lv_obj_set_size(btn_cancel, 38, 38); lv_obj_set_pos(btn_cancel, UI_W - 42, 8);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_cancel, 0, 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_t* cancel_ico = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_ico, ICO_CLOSE);
    lv_obj_set_style_text_font(cancel_ico, FONT_ICON, 0);
    lv_obj_set_style_text_color(cancel_ico, C_MUTED, 0);
    lv_obj_center(cancel_ico);

    // Textarea de edición
    lv_obj_t* ta = lv_textarea_create(overlay);
    lv_obj_set_size(ta, UI_W - 24, 38);
    lv_obj_set_pos(ta, 12, 76);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, buf_len - 1);
    if (password) lv_textarea_set_password_mode(ta, true);
    if (buf && strlen(buf) > 0) lv_textarea_set_text(ta, buf);
    _style_ta(ta);
    lv_obj_set_style_bg_color(ta, C_CARD, 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ta, C_ACCENT, 0);
    lv_obj_set_style_border_width(ta, 2, 0);
    lv_obj_set_style_radius(ta, 10, 0);
    lv_obj_set_style_text_color(ta, C_TEXT, 0);
    lv_obj_set_style_text_font(ta, FONT_MD, 0);
    lv_obj_set_style_pad_ver(ta, 12, 0);
    lv_obj_set_style_pad_hor(ta, 12, 0);
    lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);

    struct FedOvCtx { lv_obj_t* ta; lv_obj_t* overlay; char orig[VAULT_FIELD_LEN]; };
    static FedOvCtx foc;
    strncpy(foc.orig, buf ? buf : "", VAULT_FIELD_LEN-1);
    foc.ta = ta; foc.overlay = overlay;

    // Confirmar
    lv_obj_add_event_cb(btn_ok, [](lv_event_t* e){
        FedOvCtx* c = (FedOvCtx*)lv_event_get_user_data(e);
        if (c->ta && lv_obj_is_valid(c->ta))
            strncpy(_fed_ctx.buf, lv_textarea_get_text(c->ta), _fed_ctx.buf_len - 1);
        _kb_reset();
        static lv_obj_t* _del_ov; _del_ov=c->overlay;
        lv_async_call([](void*){
            if(_del_ov&&lv_obj_is_valid(_del_ov)) lv_obj_del(_del_ov);
            if (_fed_ctx.on_done) _fed_ctx.on_done();
        }, nullptr);
    }, LV_EVENT_CLICKED, &foc);

    // Cancelar — restaurar valor original
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e){
        FedOvCtx* c = (FedOvCtx*)lv_event_get_user_data(e);
        strncpy(_fed_ctx.buf, c->orig, _fed_ctx.buf_len - 1);
        _kb_reset();
        static lv_obj_t* _del_ov2; _del_ov2=c->overlay;
        lv_async_call([](void*){
            if(_del_ov2&&lv_obj_is_valid(_del_ov2)) lv_obj_del(_del_ov2);
        }, nullptr);
    }, LV_EVENT_CLICKED, &foc);

    // Teclado — OK también confirma
    lv_obj_t* fed_kb = _kb_create(overlay, ta, []{
        lv_obj_t* ta = _kb_get_ta();
        if (ta && lv_obj_is_valid(ta))
            strncpy(_fed_ctx.buf, lv_textarea_get_text(ta), _fed_ctx.buf_len - 1);
        _kb_reset();
        lv_async_call([](void*){
            lv_obj_t* scr = lv_scr_act();
            uint32_t n = lv_obj_get_child_cnt(scr);
            if (n > 0) {
                lv_obj_t* ov = lv_obj_get_child(scr, n - 1);
                if (ov) { lv_obj_del(ov); if (_fed_ctx.on_done) _fed_ctx.on_done(); }
            }
        }, nullptr);
    });
    lv_obj_set_pos(fed_kb, 0, UI_H - CKB_H);
    _kb_show();
    _kb_set_ta(ta);
}
static void _show_settings(bool animated = false);
static void _show_personalization();
static void _show_personalizacion_avanzada();
static void _show_keyboard_fullscreen(const char* field_label,
    const char* current_val, bool is_password,
    int return_form, int field_idx);
static void _show_detail(const char* name);
static void _show_qr_share(const char* title, const char* text);
static void _show_pick_type();
static void _show_cat_selector(bool is_web, lv_obj_t* cat_label);
static void _show_info();
static void _show_privacy();
static void _show_about();
static void _show_import_export();
static void _show_favorites();
static void _show_category_detail(const char* cat_id, const char* cat_name);
static void _show_categories(bool back = false);
static void _show_change_pin();
static void _show_decoy_pin_setup();
static void _show_decoy_info();
static void _show_search();
static void _show_password_generator(lv_obj_t* target_ta);

// ── Helper: pantalla base ─────────────────────────────────────
static lv_obj_t* _make_screen() {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UI_W, UI_H);
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

// ── Helper: línea separadora ──────────────────────────────────
static lv_obj_t* _divider(lv_obj_t* parent, int y) {
    lv_obj_t* line = lv_obj_create(parent);
    lv_obj_set_size(line, UI_W, 1);
    lv_obj_set_pos(line, 0, y);
    lv_obj_set_style_bg_color(line, C_BORDER, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    return line;
}

// ── Helper: botón primario ────────────────────────────────────
static lv_obj_t* _btn_primary(lv_obj_t* parent, const char* text,
                                int x, int y, int w, int h,
                                lv_event_cb_t cb, void* ud = nullptr) {
    lv_obj_t* btn = lv_btn_create(parent);
    _press_fb(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, C_ACCENT_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_SM, 0);
    lv_obj_center(lbl);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);
    return btn;
}

// ── Helper: botón outline (borde, sin relleno) ────────────────
static lv_obj_t* _btn_outline(lv_obj_t* parent, const char* text,
                                int x, int y, int w, int h,
                                lv_event_cb_t cb, void* ud = nullptr) {
    lv_obj_t* btn = lv_btn_create(parent);
    _press_fb(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn, C_TEXT, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_SM, 0);
    lv_obj_center(lbl);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);
    return btn;
}

// ── Helper: ícono inicial en círculo ─────────────────────────
static lv_obj_t* _entry_icon(lv_obj_t* parent, const char* name,
                               int x, int y, int size = 36) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_size(c, size, size);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, C_BORDER, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);
    char ini[2] = {name && name[0] ? (char)toupper(name[0]) : '?', '\0'};
    lv_obj_t* l = lv_label_create(c);
    lv_label_set_text(l, ini);
    lv_obj_set_style_text_color(l, C_TEXT, 0);
    lv_obj_set_style_text_font(l, FONT_MD, 0);
    lv_obj_center(l);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return c;
}

// ── PANTALLA: Lista ───────────────────────────────────────────
static bool _fab_open  = false;
static bool _menu_open = false;
static lv_obj_t* _btn_menu_ref = nullptr;
static lv_obj_t* _menu_panel_ref = nullptr;  // referencia al panel para drag
static lv_obj_t* _menu_overlay_ref = nullptr;
static int _menu_w = 0;

#include "ui_list.h"
#include "ui_forms.h"

#include "ui_settings.h"

// Variables globales para toggles del generador
static lv_obj_t* _gen_tog[3]   = {nullptr, nullptr, nullptr};
static lv_obj_t* _gen_thumb[3] = {nullptr, nullptr, nullptr};

static void _update_gen_toggle(int i, bool val) {
    if (i < 0 || i > 2) return;
    if (_gen_tog[i])   lv_obj_set_style_bg_color(_gen_tog[i], val ? C_ACCENT : lv_color_hex(0x555558), 0);
    if (_gen_thumb[i]) lv_obj_set_pos(_gen_thumb[i], val ? 18 : 2, 2);
}

// ── PANTALLA: Generador de contraseñas ────────────────────────

#include "ui_generator.h"

// Invalida referencias de UI antes del bloqueo del vault
void ui_main_prepare_lock() {
    _fw_kb = nullptr; _fw_btn_x = nullptr; _fw_scr = nullptr;
    _fs_kb = nullptr; _fs_btn_x = nullptr; _fs_scr = nullptr;
    _search_kb = nullptr;
    _fw_ta_pass = nullptr; _fw_ta_name = nullptr; _fw_ta_user = nullptr; _fs_ta_secret = nullptr;
    _kb_reset();
    // La barra de estado en lv_layer_top() se gestiona desde lock_screen_show()
}

#include "ui_statusbar.h"

// ── API Pública ───────────────────────────────────────────────
void ui_main_show() {
    if (!_entry_names) {
        _entry_names = (char(*)[VAULT_FIELD_LEN]) ps_malloc(sizeof(char[VAULT_FIELD_LEN]) * VAULT_MAX_ENTRIES);
        if (!_entry_names) Serial.println("[UI] ERROR: sin PSRAM para _entry_names");
    }
    if (!_filtered) {
        _filtered = (int*) ps_malloc(sizeof(int) * VAULT_MAX_ENTRIES);
        if (!_filtered) Serial.println("[UI] ERROR: sin PSRAM para _filtered");
    }
    if (!_sel_map) {
        _sel_map = (bool*) ps_malloc(sizeof(bool) * VAULT_MAX_ENTRIES);
        if (_sel_map) memset(_sel_map, 0, sizeof(bool) * VAULT_MAX_ENTRIES);
        else Serial.println("[UI] ERROR: sin PSRAM para _sel_map");
    }
    _apply_theme(vault_get_theme());
    ui_statusbar_set_visible(true); // por si el onboarding la había ocultado
    vault_cat_init_defaults();
    vault_migrate_sin_categoria();
    _show_list();
}

void ui_main_show_settings() {
    _show_settings();
}

// Arrastrar panel del menú con el dedo en tiempo real
static void _menu_animate_to(int16_t target_x, bool close_on_done) {
    if (!_menu_panel_ref || !lv_obj_is_valid(_menu_panel_ref)) return;
    lv_anim_del(_menu_panel_ref, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _menu_panel_ref);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, lv_obj_get_x(_menu_panel_ref), target_x);
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    if (close_on_done) {
        lv_anim_set_ready_cb(&a, [](lv_anim_t* an){
            lv_obj_t* panel = (lv_obj_t*)an->var;
            if (panel && lv_obj_is_valid(panel)) {
                lv_obj_t* overlay = lv_obj_get_parent(panel);
                _close_menu_overlay(overlay);
            }
        });
    }
    lv_anim_start(&a);
}

void ui_main_swipe_drag(int16_t dx) {
    // Gesto reemplazado por el botón hamburguesa — sin efecto.
    (void)dx;
}

void ui_main_swipe_right() {
    // Gesto reemplazado por el botón hamburguesa — sin efecto.
}

