#pragma once
// =============================================================
//  usb_keyboard_manager.h — Teclado USB HID para KeyVault
//  Implementación directa via USBHID sin USBHIDKeyboard.h
//  Compatible con BleKeyboard (sin conflicto de defines)
// =============================================================

#include "USB.h"
#include "USBHID.h"


int vault_get_kb_layout();

// ── Descriptor HID de teclado ─────────────────────────────────
static const uint8_t _usb_hid_desc[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    // Modifier keys
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0xE0,  // Usage Minimum (224)
    0x29, 0xE7,  // Usage Maximum (231)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x75, 0x01,  // Report Size (1)
    0x95, 0x08,  // Report Count (8)
    0x81, 0x02,  // Input (Data, Variable, Absolute)
    // Reserved
    0x95, 0x01,
    0x75, 0x08,
    0x81, 0x01,
    // Key array
    0x95, 0x06,
    0x75, 0x08,
    0x15, 0x00,
    0x25, 0x65,
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0x65,
    0x81, 0x00,
    0xC0         // End Collection
};

// ── Clase teclado USB ─────────────────────────────────────────
class USBKeyboardHID : public USBHIDDevice {
public:
    USBKeyboardHID() : hid() {}

    void begin() {
        hid.addDevice(this, sizeof(_usb_hid_desc));
        hid.begin();
    }

    uint16_t _onGetDescriptor(uint8_t* buf) override {
        memcpy(buf, _usb_hid_desc, sizeof(_usb_hid_desc));
        return sizeof(_usb_hid_desc);
    }

    void sendKey(uint8_t mod, uint8_t key) {
        uint8_t report[8] = {mod, 0, key, 0, 0, 0, 0, 0};
        hid.SendReport(0, report, 8);
        delay(10);
        memset(report, 0, 8);
        hid.SendReport(0, report, 8);
        delay(5);
    }

private:
    USBHID hid;
};

// Instancia dinámica: evita registrar la interfaz HID en TinyUSB cuando opera en modo exclusivo USB-MSC
static USBKeyboardHID* _usb_kb = nullptr;

// ── Modificadores ─────────────────────────────────────────────
#define USB_M0   0x00
#define USB_MS   0x02
#define USB_MA   0x40

#define USB_KEY_TAB    0x2B
#define USB_KEY_RETURN 0x28
#define USB_KEY_SPACE  0x2C

// ── Mapa ASCII → HID ─────────────────────────────────────────
static void _usb_send_char(char c) {
    if (!_usb_kb) return; // modo MSC u otro estado sin teclado USB activo
    uint8_t mod = USB_M0, key = 0;
    int layout = vault_get_kb_layout(); // 0=ES-LAT, 1=ES-ES, 2=US

    if (c >= 'a' && c <= 'z') { _usb_kb->sendKey(USB_M0, c-'a'+4); return; }
    if (c >= 'A' && c <= 'Z') { _usb_kb->sendKey(USB_MS, c-'A'+4); return; }
    if (c == ' ')  { _usb_kb->sendKey(USB_M0, USB_KEY_SPACE);  return; }
    if (c == '\n') { _usb_kb->sendKey(USB_M0, USB_KEY_RETURN); return; }
    if (c == '\t') { _usb_kb->sendKey(USB_M0, USB_KEY_TAB);    return; }

    if (layout == 0) {
        // ES-LAT
        switch (c) {
            case '1': key=30; break; case '!': mod=USB_MS; key=30; break;
            case '2': key=31; break; case '"': mod=USB_MS; key=31; break;
            case '3': key=32; break; case '#': mod=USB_MS; key=32; break;
            case '4': key=33; break; case '$': mod=USB_MS; key=33; break;
            case '5': key=34; break; case '%': mod=USB_MS; key=34; break;
            case '6': key=35; break; case '&': mod=USB_MS; key=35; break;
            case '7': key=36; break; case '/': mod=USB_MS; key=36; break;
            case '8': key=37; break; case '(': mod=USB_MS; key=37; break;
            case '9': key=38; break; case ')': mod=USB_MS; key=38; break;
            case '0': key=39; break; case '=': mod=USB_MS; key=39; break;
            case '\'': key=45; break; case '?':  mod=USB_MS; key=45; break;
            case '\\': mod=USB_MA; key=45; break;
            case '+':  key=48; break; case '*':  mod=USB_MS; key=48; break;
            case '~':  mod=USB_MA; key=48; break;
            case '{':  key=52; break; case '[':  mod=USB_MS; key=52; break;
            case '^':  mod=USB_MA; key=52; break;
            case '}':  key=50; break; case ']':  mod=USB_MS; key=50; break;
            case '|':  key=53; break; case '°':  mod=USB_MS; key=53; break;
            case ',':  key=54; break; case ';':  mod=USB_MS; key=54; break;
            case '.':  key=55; break; case ':':  mod=USB_MS; key=55; break;
            case '-':  key=56; break; case '_':  mod=USB_MS; key=56; break;
            case '@':  mod=USB_MA; key=20; break;
            case '<':  key=100; break; case '>': mod=USB_MS; key=100; break;
        }
    } else if (layout == 1) {
        // ES-ES
        switch (c) {
            case '1': key=30; break; case '!': mod=USB_MS; key=30; break; case '|': mod=USB_MA; key=30; break;
            case '2': key=31; break; case '"': mod=USB_MS; key=31; break; case '@': mod=USB_MA; key=31; break;
            case '3': key=32; break; case '#': mod=USB_MA; key=32; break;
            case '4': key=33; break; case '$': mod=USB_MS; key=33; break; case '~': mod=USB_MA; key=33; break;
            case '5': key=34; break; case '%': mod=USB_MS; key=34; break;
            case '6': key=35; break; case '&': mod=USB_MS; key=35; break;
            case '7': key=36; break; case '/': mod=USB_MS; key=36; break;
            case '8': key=37; break; case '(': mod=USB_MS; key=37; break;
            case '9': key=38; break; case ')': mod=USB_MS; key=38; break;
            case '0': key=39; break; case '=': mod=USB_MS; key=39; break;
            case '\'': key=45; break; case '?': mod=USB_MS; key=45; break;
            case '+':  key=48; break; case '*': mod=USB_MS; key=48; break; case ']': mod=USB_MA; key=48; break;
            case '´':  key=52; break; case '¨': mod=USB_MS; key=52; break; case '{': mod=USB_MA; key=52; break;
            case '}':  mod=USB_MA; key=50; break;
            case '\\': mod=USB_MA; key=53; break; case '[': mod=USB_MA; key=47; break;
            case ',':  key=54; break; case ';': mod=USB_MS; key=54; break;
            case '.':  key=55; break; case ':': mod=USB_MS; key=55; break;
            case '-':  key=56; break; case '_': mod=USB_MS; key=56; break;
            case '<':  key=100; break; case '>': mod=USB_MS; key=100; break;
        }
    } else {
        // US
        switch (c) {
            case '1': key=30; break; case '!': mod=USB_MS; key=30; break;
            case '2': key=31; break; case '@': mod=USB_MS; key=31; break;
            case '3': key=32; break; case '#': mod=USB_MS; key=32; break;
            case '4': key=33; break; case '$': mod=USB_MS; key=33; break;
            case '5': key=34; break; case '%': mod=USB_MS; key=34; break;
            case '6': key=35; break; case '^': mod=USB_MS; key=35; break;
            case '7': key=36; break; case '&': mod=USB_MS; key=36; break;
            case '8': key=37; break; case '*': mod=USB_MS; key=37; break;
            case '9': key=38; break; case '(': mod=USB_MS; key=38; break;
            case '0': key=39; break; case ')': mod=USB_MS; key=39; break;
            case '-':  key=45; break; case '_': mod=USB_MS; key=45; break;
            case '=':  key=46; break; case '+': mod=USB_MS; key=46; break;
            case '[':  key=47; break; case '{': mod=USB_MS; key=47; break;
            case ']':  key=48; break; case '}': mod=USB_MS; key=48; break;
            case '\\': key=49; break; case '|': mod=USB_MS; key=49; break;
            case ';':  key=51; break; case ':': mod=USB_MS; key=51; break;
            case '\'': key=52; break; case '"': mod=USB_MS; key=52; break;
            case '`':  key=53; break; case '~': mod=USB_MS; key=53; break;
            case ',':  key=54; break; case '<': mod=USB_MS; key=54; break;
            case '.':  key=55; break; case '>': mod=USB_MS; key=55; break;
            case '/':  key=56; break; case '?': mod=USB_MS; key=56; break;
        }
    }
    if (key) _usb_kb->sendKey(mod, key);
}

// ── API pública ───────────────────────────────────────────────
void usb_kb_begin() {
    if (!_usb_kb) _usb_kb = new USBKeyboardHID();
    USB.manufacturerName("Huskey");
    USB.productName("Huskey Pocket");
    _usb_kb->begin();
    USB.begin();
}

void usb_type_string(const char* str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) { _usb_send_char(str[i]); delay(5); }
}

void usb_type_tab()   { if (_usb_kb) _usb_kb->sendKey(USB_M0, USB_KEY_TAB); }
void usb_type_enter() { if (_usb_kb) _usb_kb->sendKey(USB_M0, USB_KEY_RETURN); }

void usb_type_user(const char* u)   { usb_type_string(u); }
void usb_type_pass(const char* p)   { usb_type_string(p); }
void usb_type_secret(const char* s) { usb_type_string(s); }
