#pragma once
// =============================================================
//  usb_msc_manager.h — Modo almacenamiento masivo USB (USBMSC sobre SD_MMC).
//  Modo exclusivo: el dispositivo opera únicamente como unidad de almacenamiento.
//  Salida mediante botón en pantalla o desconexión del cable USB.
// =============================================================
#include <lvgl.h>
#include "USB.h"
#include "USBMSC.h"
#include "SD_MMC.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "vault.h"
#include "sd_manager.h"
#include "fonts.h"
#include "i18n.h"

static USBMSC        _msc;
static sdmmc_card_t*  _msc_card = nullptr;
static lv_obj_t*      _msc_scr  = nullptr;

// Acceso al puntero sdmmc_card_t protegido en SDMMCFS
class _SDMMCExpose : public fs::SDMMCFS {
public:
    sdmmc_card_t* exposedCard() { return _card; }
};
static sdmmc_card_t* _msc_get_card() {
    return ((_SDMMCExpose*)&SD_MMC)->exposedCard();
}

// ── Callbacks de bloque crudo — el USBMSC no conoce el filesystem,
//    solo lee/escribe sectores. lba=sector inicial, offset siempre 0
//    en uso normal (bufsize coincide con sector_size por llamada).
static bool _msc_read_only = false; // si true, se rechazan las escrituras del host

static int32_t _msc_on_read(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    if (!_msc_card) return -1;
    uint32_t sector_size = _msc_card->csd.sector_size;
    if (sector_size == 0) return -1;                 // CSD inválido: evita div/0
    if (offset != 0) return -1;                      // solo transferencias alineadas a sector
    if (bufsize == 0 || (bufsize % sector_size) != 0) return -1;
    uint32_t sectors = bufsize / sector_size;
    // Rango: el host no debe leer más allá de la capacidad de la tarjeta.
    if ((uint64_t)lba + sectors > (uint64_t)_msc_card->csd.capacity) return -1;
    if (sdmmc_read_sectors(_msc_card, buffer, lba, sectors) != ESP_OK) return -1;
    return bufsize;
}
static int32_t _msc_on_write(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    if (!_msc_card) return -1;
    if (_msc_read_only) return -1;                   // modo solo-lectura: rechazar escritura
    uint32_t sector_size = _msc_card->csd.sector_size;
    if (sector_size == 0) return -1;                 // CSD inválido: evita div/0
    if (offset != 0) return -1;                      // no escribir con desfase (corrompería el sector)
    if (bufsize == 0 || (bufsize % sector_size) != 0) return -1;
    uint32_t sectors = bufsize / sector_size;
    if ((uint64_t)lba + sectors > (uint64_t)_msc_card->csd.capacity) return -1;
    if (sdmmc_write_sectors(_msc_card, buffer, lba, sectors) != ESP_OK) return -1;
    return bufsize;
}
static bool _msc_on_start_stop(uint8_t power_condition, bool start, bool load_eject) {
    return true; // nada que expulsar físicamente, siempre aceptar
}

// Salir del modo USB MSC: restablecer preferencia y reiniciar
static void _msc_exit_now() {
    vault_set_usb_msc_mode(false);
    Serial.println("[MSC] Saliendo del modo USB, reiniciando...");
    delay(150);
    ESP.restart();
}

static void _msc_usb_event_cb(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (id == ARDUINO_USB_SUSPEND_EVENT) {
        Serial.println("[MSC] USB desconectado (suspend)");
        _msc_exit_now();
    }
}

// ── Pantalla mínima — sin vault, sin lista, solo estado + botón ──
static void usb_msc_show_screen(uint32_t sector_size, uint64_t sector_count) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, t(K_MODO_USB_ACTIVO));
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -70);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, t(K_USB_MSC_DESC));
    lv_obj_set_style_text_color(sub, lv_color_hex(0x8E8E93), 0);
    lv_obj_set_style_text_font(sub, FONT_SM, 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t* btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 170, 48);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 100);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xEE3333), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, t(K_SALIR_MODO_USB));
    lv_obj_set_style_text_font(lbl, FONT_SM, 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, [](lv_event_t* e) { _msc_exit_now(); }, LV_EVENT_CLICKED, nullptr);

    lv_scr_load(scr);
    _msc_scr = scr;
}

// ── Arranque exclusivo del modo USB — llamar desde setup() en vez
//    de inicializar vault/WiFi/teclado. Asume LCD+touch+LVGL ya
//    inicializados por el caller (mismo bloque de setup() de siempre).
void usb_msc_begin() {
    if (!sd_begin()) {
        Serial.println("[MSC] Error: no se pudo montar SD, abortando modo USB");
        vault_set_usb_msc_mode(false);
        delay(200);
        ESP.restart();
        return;
    }
    _msc_card = _msc_get_card();
    if (!_msc_card) {
        Serial.println("[MSC] Error: sin handle de tarjeta SD");
        vault_set_usb_msc_mode(false);
        delay(200);
        ESP.restart();
        return;
    }

    uint32_t sector_size  = _msc_card->csd.sector_size;
    uint64_t sector_count = (uint64_t)_msc_card->csd.capacity;

    // sector_size == 0 haría div/0 en los callbacks de bloque — abortar limpio.
    if (sector_size == 0) {
        Serial.println("[MSC] Error: sector_size = 0, CSD invalido, abortando");
        vault_set_usb_msc_mode(false);
        delay(200);
        ESP.restart();
        return;
    }

    // Cargar preferencia de solo-lectura (por defecto lectura-escritura).
    _msc_read_only = vault_get_usb_msc_readonly();

    // Las SD reales son prácticamente siempre 512B/sector nativo —no se
    // traduce nada aquí porque los callbacks onRead/onWrite usan
    // sdmmc_read/write_sectors con el sector nativo de la tarjeta; una
    // "traducción" de unidades ahí sería más riesgo que beneficio. Si
    // sector_size llegara a no ser 512, el diagnóstico en pantalla lo
    // muestra para detectarlo.
    if (sector_size != 512) {
        Serial.printf("[MSC] ADVERTENCIA: sector_size nativo = %u (no 512)\n", sector_size);
    }

    _msc.vendorID("Huskey");
    _msc.productID("Pocket");
    _msc.productRevision("1.00"); // 4 caracteres — algunos stacks de Windows son estrictos con el formato
    _msc.onRead(_msc_on_read);
    _msc.onWrite(_msc_on_write);
    _msc.onStartStop(_msc_on_start_stop);
    _msc.mediaPresent(true);
    _msc.begin(sector_count, sector_size);

    USB.onEvent(_msc_usb_event_cb);
    USB.begin();

    Serial.printf("[MSC] Disco USB listo - %llu sectores x %u bytes\n", sector_count, sector_size);
    usb_msc_show_screen(sector_size, sector_count);
}
