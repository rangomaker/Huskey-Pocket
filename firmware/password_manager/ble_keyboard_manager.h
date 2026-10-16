#pragma once
// =============================================================
//  ble_keyboard_manager.h — Teclado Bluetooth HID para Huskey
//  ESP32-S3, NimBLE-Arduino 2.x
//  Mapas basados en QMK keymaps oficiales
// =============================================================

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEHIDDevice.h>
#include <HIDTypes.h>
#include <lvgl.h>
#include "colors.h"   // tema (C_*) — este header se incluye primero en el .ino,
#include "fonts.h"    // así que trae sus propias dependencias de UI
#include "i18n.h"

// Forward declaration
int vault_get_kb_layout();

static const uint8_t _ble_hid_desc[] = {
    0x05,0x01,0x09,0x06,0xA1,0x01,0x85,0x01,
    0x05,0x07,0x19,0xE0,0x29,0xE7,
    0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,
    0x95,0x01,0x75,0x08,0x81,0x01,
    0x95,0x06,0x75,0x08,0x15,0x00,0x25,0x65,
    0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00,
    0xC0
};

#define M0   0x00
#define MS   0x02
#define MA   0x40

#define BLE_KEY_TAB    0x2B
#define BLE_KEY_RETURN 0x28
#define BLE_KEY_SPACE  0x2C

static NimBLEServer*         _ble_server    = nullptr;
static NimBLEHIDDevice*      _ble_hid       = nullptr;
static NimBLECharacteristic* _ble_input     = nullptr;
static bool                  _ble_started   = false;
static bool                  _ble_connected = false;
static bool                  _ble_authenticated = false; // enlace cifrado+autenticado

// ── Confirmación de emparejamiento en pantalla (Numeric Comparison) ──
static bool           _ble_pairing_pending = false;
static NimBLEConnInfo* _ble_pending_conn = nullptr;

// Sincronización entre la tarea NimBLE y el hilo UI (loopTask)
static portMUX_TYPE     _ble_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool    _ble_req_show   = false;
static volatile bool    _ble_req_cancel = false;
static uint32_t         _ble_req_passkey = 0;
static NimBLEConnInfo*  _ble_req_conn = nullptr;
static lv_obj_t*      _ble_pairing_scr = nullptr;
static lv_obj_t*      _ble_prev_scr    = nullptr; // pantalla activa antes del popup

static void _ble_pairing_respond(bool accepted) {
    if (!_ble_pairing_pending) return;
    if (_ble_pending_conn) {
        NimBLEDevice::injectConfirmPasskey(*_ble_pending_conn, accepted);
        delete _ble_pending_conn;
        _ble_pending_conn = nullptr;
    }
    _ble_pairing_pending = false;

    lv_obj_t* popup = _ble_pairing_scr;
    _ble_pairing_scr = nullptr;
    if (_ble_prev_scr && lv_obj_is_valid(_ble_prev_scr)) {
        lv_scr_load(_ble_prev_scr); // restaura la pantalla que estaba activa
    }
    _ble_prev_scr = nullptr;
    if (popup) lv_obj_del_async(popup);
}

// Cancela un popup de emparejamiento pendiente SIN inyectar respuesta al
// stack (la conexión ya murió). Evita la fuga del NimBLEConnInfo y el
// puntero colgante si el peer se desconecta con el popup abierto.
static void _ble_pairing_cancel_pending() {
    if (!_ble_pairing_pending && !_ble_pending_conn && !_ble_pairing_scr) return;
    if (_ble_pending_conn) { delete _ble_pending_conn; _ble_pending_conn = nullptr; }
    _ble_pairing_pending = false;

    lv_obj_t* popup = _ble_pairing_scr;
    _ble_pairing_scr = nullptr;
    if (_ble_prev_scr && lv_obj_is_valid(_ble_prev_scr)) {
        lv_scr_load(_ble_prev_scr);
    }
    _ble_prev_scr = nullptr;
    if (popup) lv_obj_del_async(popup);
}

// Popup mínimo y autocontenido — no depende de tokens de tema del
// resto de la app (mismo criterio que la pantalla de usb_msc_manager.h)
// para no requerir includes adicionales aquí.
static void _ble_pairing_show(uint32_t passkey) {
    _ble_prev_scr = lv_scr_act();

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, C_BG, 0); // respeta el tema activo
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, t(K_BLE_PAIR_TITULO));
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, 220);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -90);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, t(K_BLE_PAIR_MSG));
    lv_obj_set_style_text_color(sub, C_MUTED, 0);
    lv_obj_set_style_text_font(sub, FONT_SM, 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, -35);

    lv_obj_t* code = lv_label_create(scr);
    char code_str[8];
    snprintf(code_str, sizeof(code_str), "%06u", (unsigned)(passkey % 1000000));
    lv_label_set_text(code, code_str);
    lv_obj_set_style_text_color(code, C_TEXT, 0);
    lv_obj_set_style_text_font(code, FONT_HDR_LG, 0);
    lv_obj_align(code, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t* btn_no = lv_btn_create(scr);
    lv_obj_set_size(btn_no, 90, 44);
    lv_obj_align(btn_no, LV_ALIGN_CENTER, -55, 70);
    lv_obj_set_style_bg_color(btn_no, C_CARD, 0);
    lv_obj_set_style_radius(btn_no, 10, 0);
    lv_obj_set_style_border_width(btn_no, 0, 0);  // sin el borde default de lv_btn
    lv_obj_set_style_shadow_width(btn_no, 0, 0);
    lv_obj_t* no_lbl = lv_label_create(btn_no);
    lv_label_set_text(no_lbl, t(K_NO));
    lv_obj_set_style_text_color(no_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(no_lbl, FONT_SM, 0);
    lv_obj_center(no_lbl);
    lv_obj_add_event_cb(btn_no, [](lv_event_t* e){ _ble_pairing_respond(false); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_yes = lv_btn_create(scr);
    lv_obj_set_size(btn_yes, 90, 44);
    lv_obj_align(btn_yes, LV_ALIGN_CENTER, 55, 70);
    lv_obj_set_style_bg_color(btn_yes, C_ACCENT, 0); // acento del tema
    lv_obj_set_style_radius(btn_yes, 10, 0);
    lv_obj_set_style_border_width(btn_yes, 0, 0);
    lv_obj_set_style_shadow_width(btn_yes, 0, 0);
    lv_obj_t* yes_lbl = lv_label_create(btn_yes);
    lv_label_set_text(yes_lbl, t(K_SI));
    lv_obj_set_style_text_color(yes_lbl, C_ACCENT_TEXT, 0);
    lv_obj_set_style_text_font(yes_lbl, FONT_SM, 0);
    lv_obj_center(yes_lbl);
    lv_obj_add_event_cb(btn_yes, [](lv_event_t* e){ _ble_pairing_respond(true); }, LV_EVENT_CLICKED, nullptr);

    lv_scr_load(scr);
    _ble_pairing_scr = scr;
}

class BleKbCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& i) override {
        _ble_connected = true;
        _ble_authenticated = i.isEncrypted() && i.isAuthenticated();
        Serial.println("[BLE] Conectado");
    }
    void onDisconnect(NimBLEServer* s, NimBLEConnInfo& i, int r) override {
        _ble_connected = false;
        _ble_authenticated = false;
        // Si había un popup de emparejamiento abierto para esta conexión,
        // cancelarlo: el peer se fue y el NimBLEConnInfo ya no es válido.
        _ble_req_cancel = true; // tarea BLE: no tocar LVGL aquí — lo procesa ble_kb_tick()
        NimBLEDevice::startAdvertising();
    }

    // Se llama al completar (o fallar) el emparejamiento seguro. Aquí es
    // donde el enlace pasa a estar realmente autenticado y cifrado.
    void onAuthenticationComplete(NimBLEConnInfo& i) override {
        _ble_authenticated = i.isEncrypted() && i.isAuthenticated();
        Serial.printf("[BLE] Auth completa — cifrado=%d autenticado=%d\n",
                      i.isEncrypted(), i.isAuthenticated());
    }

    // Passkey Entry no soportado — responder valor aleatorio para rechazar
    uint32_t onPassKeyDisplay() override {
        uint32_t r = esp_random() % 1000000;
        Serial.println("[BLE] Peer pidio Passkey Entry — no soportado, rechazando");
        return r;
    }

    // Numeric Comparison: delegar confirmación de código a ble_kb_tick() en loopTask
    void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pass_key) override {
        NimBLEConnInfo* nc = new NimBLEConnInfo(connInfo);
        NimBLEConnInfo* viejo = nullptr;
        portENTER_CRITICAL(&_ble_mux);
        viejo = _ble_req_conn;
        _ble_req_conn = nc;
        _ble_req_passkey = pass_key;
        _ble_req_show = true;
        portEXIT_CRITICAL(&_ble_mux);
        if (viejo) delete viejo;
        Serial.printf("[BLE] Confirmar emparejamiento — codigo: %06u\n", (unsigned)pass_key);
    }
};
static BleKbCB _ble_cb;



void ble_kb_begin() {
    if (_ble_started) return;
    NimBLEDevice::init("Huskey Pocket");
    // DisplayYesNo (Numeric Comparison) en vez del default anterior
    // (sin IO capability = Just Works, sin confirmación posible).
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);
    NimBLEDevice::setSecurityAuth(true,true,true);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    _ble_server = NimBLEDevice::createServer();
    _ble_server->setCallbacks(&_ble_cb, false); // false = NimBLE no debe hacer delete — _ble_cb es estático, no heap
    _ble_hid = new NimBLEHIDDevice(_ble_server);
    _ble_hid->setManufacturer("RangoMaker");
    _ble_hid->setPnp(0x02,0x045E,0x0750,0x0300);
    _ble_hid->setHidInfo(0x00,0x01);
    _ble_input = _ble_hid->getInputReport(1);
    _ble_hid->setReportMap((uint8_t*)_ble_hid_desc,sizeof(_ble_hid_desc));
    _ble_hid->startServices();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAppearance(HID_KEYBOARD);
    adv->addServiceUUID(_ble_hid->getHidService()->getUUID());
    adv->setName("Huskey Pocket");
    adv->start();
    _ble_started = true;
    Serial.println("[BLE] Teclado iniciado");
}

// Procesa en el loopTask los pedidos dejados por la tarea BLE.
// Llamar en cada vuelta de loop().
void ble_kb_tick() {
    if (_ble_req_cancel) {
        _ble_req_cancel = false;
        _ble_pairing_cancel_pending();
    }
    bool show = false; uint32_t pk = 0; NimBLEConnInfo* c = nullptr;
    portENTER_CRITICAL(&_ble_mux);
    if (_ble_req_show) {
        show = true; pk = _ble_req_passkey;
        c = _ble_req_conn; _ble_req_conn = nullptr;
        _ble_req_show = false;
    }
    portEXIT_CRITICAL(&_ble_mux);
    if (show) {
        _ble_pairing_cancel_pending(); // limpiar popup previo si lo hubiera
        _ble_pending_conn = c;
        _ble_pairing_pending = true;
        _ble_pairing_show(pk);
    }
}

void ble_kb_end() {
    if (!_ble_started) return;
    // Limpieza de un emparejamiento a medias (popup + NimBLEConnInfo):
    // ble_kb_end se llama desde el loopTask (lock_screen_show), así que
    // aquí sí es seguro tocar LVGL.
    _ble_pairing_cancel_pending();
    portENTER_CRITICAL(&_ble_mux);
    NimBLEConnInfo* c = _ble_req_conn; _ble_req_conn = nullptr;
    _ble_req_show = false; _ble_req_cancel = false;
    portEXIT_CRITICAL(&_ble_mux);
    if (c) delete c;
    NimBLEDevice::deinit(true);
    _ble_started=false; _ble_connected=false;
}

// Borra TODOS los vínculos (bonds) BLE. La API deleteAllBonds() de esta
// versión de NimBLE está rota (siempre false), así que se borra su
// almacenamiento NVS directamente — namespace "nimble_bond" del port
// ESP-IDF — con el stack apagado, y se rearranca limpio.
#include <Preferences.h>
bool ble_kb_delete_all_bonds() {
    bool estaba = _ble_started;
    // Desconectar limpiamente a cualquier peer ANTES de tumbar el stack:
    // sin esto, el deinit corta el enlace en seco y de este lado queda
    // estado de conexión fantasma. (El peer conservará SU mitad del bond
    // e intentará reconectar — eso es del protocolo; el usuario debe
    // eliminar el dispositivo también del otro lado.)
    if (estaba && _ble_server && _ble_connected) {
        int n = _ble_server->getConnectedCount();
        for (int i = n - 1; i >= 0; i--) {
            NimBLEConnInfo ci = _ble_server->getPeerInfo(i);
            _ble_server->disconnect(ci.getConnHandle());
        }
        delay(250); // dar tiempo al stack a cerrar el enlace antes del deinit
    }
    if (estaba) ble_kb_end(); // nunca tocar el NVS del stack con él vivo
    Preferences p;
    bool ok = p.begin("nimble_bond", false);
    if (ok) { p.clear(); p.end(); }
    if (estaba) ble_kb_begin(); // rearranca con la tabla de bonds vacía
    Serial.printf("[BLE] Bonds borrados via NVS: %s\n", ok ? "OK" : "FALLO");
    return ok;
}

bool ble_kb_connected() { return _ble_started && _ble_connected; }
bool ble_kb_is_init()   { return _ble_started; }
// Enlace listo para transmitir credenciales: conectado Y con emparejamiento
// seguro completado (cifrado + autenticado). Todos los envíos HID reales
// dependen de esto, no solo de tener una conexión física abierta.
bool ble_kb_secure()    { return _ble_started && _ble_connected && _ble_authenticated; }

static void _ble_send_key(uint8_t mod, uint8_t key) {
    // Nunca emitir un reporte HID sobre un enlace sin cifrar/autenticar:
    // en BLE la conexión existe antes de completar el emparejamiento seguro.
    if (!ble_kb_secure() || !_ble_input) return;
    uint8_t r[8]={mod,0,key,0,0,0,0,0};
    _ble_input->setValue(r,8); _ble_input->notify(); delay(10);
    memset(r,0,8); _ble_input->setValue(r,8); _ble_input->notify(); delay(5);
}

static void _send_char(char c) {
    uint8_t mod=M0, key=0;
    int layout = vault_get_kb_layout(); // 0=ES-LAT, 1=ES-ES, 2=US

    if (c>='a'&&c<='z') { _ble_send_key(M0,c-'a'+4); return; }
    if (c>='A'&&c<='Z') { _ble_send_key(MS,c-'A'+4); return; }
    if (c==' ')  { _ble_send_key(M0,BLE_KEY_SPACE);  return; }
    if (c=='\n') { _ble_send_key(M0,BLE_KEY_RETURN); return; }
    if (c=='\t') { _ble_send_key(M0,BLE_KEY_TAB);    return; }

    if (layout == 0) {
        // ── ES-LAT (Spanish Latin America) — QMK keymap ──────
        switch (c) {
            case '1': key=30; break; case '!': mod=MS; key=30; break;
            case '2': key=31; break; case '"': mod=MS; key=31; break;
            case '3': key=32; break; case '#': mod=MS; key=32; break;
            case '4': key=33; break; case '$': mod=MS; key=33; break;
            case '5': key=34; break; case '%': mod=MS; key=34; break;
            case '6': key=35; break; case '&': mod=MS; key=35; break;
            case '7': key=36; break; case '/': mod=MS; key=36; break;
            case '8': key=37; break; case '(': mod=MS; key=37; break;
            case '9': key=38; break; case ')': mod=MS; key=38; break;
            case '0': key=39; break; case '=': mod=MS; key=39; break;
            case '\'': key=45; break; case '?':  mod=MS; key=45; break;
            case '\\': mod=MA; key=45; break;
            case '+':  key=48; break; case '*':  mod=MS; key=48; break;
            case '~':  mod=MA; key=48; break;
            case '{':  key=52; break; case '[':  mod=MS; key=52; break;
            case '^':  mod=MA; key=52; break;
            case '}':  key=50; break; case ']':  mod=MS; key=50; break;
            case '|':  key=53; break; case '°':  mod=MS; key=53; break;
            case ',':  key=54; break; case ';':  mod=MS; key=54; break;
            case '.':  key=55; break; case ':':  mod=MS; key=55; break;
            case '-':  key=56; break; case '_':  mod=MS; key=56; break;
            case '@':  mod=MA; key=20; break;  // AltGr+Q
            case '<':  key=100; break; case '>': mod=MS; key=100; break;
        }
    } else if (layout == 1) {
        // ── ES-ES (Spanish Spain) — QMK keymap ───────────────
        switch (c) {
            case '1': key=30; break; case '!': mod=MS; key=30; break; case '|': mod=MA; key=30; break;
            case '2': key=31; break; case '"': mod=MS; key=31; break; case '@': mod=MA; key=31; break;
            case '3': key=32; break; case '#': mod=MA; key=32; break;
            case '4': key=33; break; case '$': mod=MS; key=33; break; case '~': mod=MA; key=33; break;
            case '5': key=34; break; case '%': mod=MS; key=34; break;
            case '6': key=35; break; case '&': mod=MS; key=35; break;
            case '7': key=36; break; case '/': mod=MS; key=36; break;
            case '8': key=37; break; case '(': mod=MS; key=37; break;
            case '9': key=38; break; case ')': mod=MS; key=38; break;
            case '0': key=39; break; case '=': mod=MS; key=39; break;
            case '\'': key=45; break; case '?': mod=MS; key=45; break;
            case '+':  key=48; break; case '*': mod=MS; key=48; break; case ']': mod=MA; key=48; break;
            case '´':  key=52; break; case '¨': mod=MS; key=52; break; case '{': mod=MA; key=52; break;
            case '}':  mod=MA; key=50; break;
            case '\\': mod=MA; key=53; break; case '[': mod=MA; key=47; break;
            case ',':  key=54; break; case ';': mod=MS; key=54; break;
            case '.':  key=55; break; case ':': mod=MS; key=55; break;
            case '-':  key=56; break; case '_': mod=MS; key=56; break;
            case '<':  key=100; break; case '>': mod=MS; key=100; break;
        }
    } else {
        // ── US layout ─────────────────────────────────────────
        switch (c) {
            case '1': key=30; break; case '!': mod=MS; key=30; break;
            case '2': key=31; break; case '@': mod=MS; key=31; break;
            case '3': key=32; break; case '#': mod=MS; key=32; break;
            case '4': key=33; break; case '$': mod=MS; key=33; break;
            case '5': key=34; break; case '%': mod=MS; key=34; break;
            case '6': key=35; break; case '^': mod=MS; key=35; break;
            case '7': key=36; break; case '&': mod=MS; key=36; break;
            case '8': key=37; break; case '*': mod=MS; key=37; break;
            case '9': key=38; break; case '(': mod=MS; key=38; break;
            case '0': key=39; break; case ')': mod=MS; key=39; break;
            case '-':  key=45; break; case '_': mod=MS; key=45; break;
            case '=':  key=46; break; case '+': mod=MS; key=46; break;
            case '[':  key=47; break; case '{': mod=MS; key=47; break;
            case ']':  key=48; break; case '}': mod=MS; key=48; break;
            case '\\': key=49; break; case '|': mod=MS; key=49; break;
            case ';':  key=51; break; case ':': mod=MS; key=51; break;
            case '\'': key=52; break; case '"': mod=MS; key=52; break;
            case '`':  key=53; break; case '~': mod=MS; key=53; break;
            case ',':  key=54; break; case '<': mod=MS; key=54; break;
            case '.':  key=55; break; case '>': mod=MS; key=55; break;
            case '/':  key=56; break; case '?': mod=MS; key=56; break;
        }
    }
    if (key) _ble_send_key(mod, key);
}

void ble_type_string(const char* str) {
    if (!ble_kb_secure()||!str) return;
    for (int i=0;str[i];i++) { _send_char(str[i]); delay(5); }
}

void ble_type_tab()   { _ble_send_key(M0,BLE_KEY_TAB); }
void ble_type_enter() { _ble_send_key(M0,BLE_KEY_RETURN); }

// ── Gestión de vínculos (bonds) ─────────────────────────────────
int ble_kb_num_bonds() {
    if (!_ble_started) return 0;
    return NimBLEDevice::getNumBonds();
}

#ifdef HUSKEY_BLE_DIAGNOSTIC
void ble_type_diagnostic() {
    if (!ble_kb_secure()) return;
    for (uint8_t k=30;k<=56;k++) { _ble_send_key(M0,k); delay(80); }
    _ble_send_key(M0,BLE_KEY_RETURN); delay(300);
    for (uint8_t k=30;k<=56;k++) { _ble_send_key(MS,k); delay(80); }
}
#else
void ble_type_diagnostic() { }
#endif

void ble_type_user(const char* u)   { ble_type_string(u); }
void ble_type_pass(const char* p)   { ble_type_string(p); }
void ble_type_secret(const char* s) { ble_type_string(s); }
