// ============================================================
//  screenshot.h — Capturas de pantalla LVGL a formato BMP en SD
//  Comandos serie: shot (1x), shot2 (2x), shot3 (3x) -> /export/screen_NN.bmp
// ============================================================
#pragma once
#include <lvgl.h>
#include <SD_MMC.h>

// Escribe la cabecera BMP de 54 bytes (24 bpp, sin compresión)
static void _shot_bmp_header(File& f, int w, int h) {
    uint32_t row_bytes = ((w * 3 + 3) / 4) * 4; // filas alineadas a 4 bytes
    uint32_t img_size  = row_bytes * h;
    uint32_t file_size = 54 + img_size;
    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(hdr + 2,  &file_size, 4);
    uint32_t off = 54;           memcpy(hdr + 10, &off, 4);
    uint32_t dib = 40;           memcpy(hdr + 14, &dib, 4);
    int32_t  iw  = w;            memcpy(hdr + 18, &iw, 4);
    int32_t  ih  = h;            memcpy(hdr + 22, &ih, 4); // positivo = bottom-up
    uint16_t planes = 1;         memcpy(hdr + 26, &planes, 2);
    uint16_t bpp    = 24;        memcpy(hdr + 28, &bpp, 2);
    memcpy(hdr + 34, &img_size, 4);
    f.write(hdr, 54);
}

// Toma la captura y la escribe a la SD. Retorna true si se guardó.
// scale: 1-3 — cada píxel se replica en un bloque scale x scale (nítido).
static bool screenshot_take(int scale = 1) {
    if (scale < 1) scale = 1;
    if (scale > 3) scale = 3;
    lv_obj_t* scr = lv_scr_act();
    if (!scr) return false;

    uint32_t buf_size = lv_snapshot_buf_size_needed(scr, LV_IMG_CF_TRUE_COLOR);
    if (buf_size == 0) return false;

    // Buffer en PSRAM: ~150 KB para 240x320x16bpp — no cabe en heap interno
    uint8_t* buf = (uint8_t*)ps_malloc(buf_size);
    if (!buf) { Serial.println("[SHOT] sin PSRAM para el buffer"); return false; }

    lv_img_dsc_t dsc;
    if (lv_snapshot_take_to_buf(scr, LV_IMG_CF_TRUE_COLOR, &dsc, buf, buf_size) != LV_RES_OK) {
        Serial.println("[SHOT] snapshot falló");
        free(buf);
        return false;
    }
    int w = dsc.header.w, h = dsc.header.h;
    int ow = w * scale, oh = h * scale; // dimensiones de salida

    // Primer nombre libre screen_00..screen_99
    SD_MMC.mkdir("/export");
    char path[40];
    int idx = 0;
    for (; idx < 100; idx++) {
        snprintf(path, sizeof(path), "/export/screen_%02d.bmp", idx);
        if (!SD_MMC.exists(path)) break;
    }
    if (idx >= 100) { Serial.println("[SHOT] 100 capturas — limpia /export"); free(buf); return false; }

    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) { Serial.println("[SHOT] no se pudo abrir el archivo"); free(buf); return false; }

    _shot_bmp_header(f, ow, oh);

    // BMP es bottom-up: escribir filas de abajo hacia arriba, BGR888.
    // lv_color_to32 maneja el formato nativo (incluido LV_COLOR_16_SWAP).
    // Escalado BILINEAL (punto fijo 8 bits): suaviza bordes y curvas al
    // ampliar — el panel es 240x320, no hay detalle extra que recuperar,
    // pero la interpolacion evita el pixelado de la replicacion.
    const lv_color_t* px = (const lv_color_t*)dsc.data;
    uint32_t row_bytes = ((ow * 3 + 3) / 4) * 4;
    uint8_t* row = (uint8_t*)malloc(row_bytes);
    if (!row) { f.close(); SD_MMC.remove(path); free(buf); return false; }
    memset(row, 0, row_bytes);

    for (int oy = oh - 1; oy >= 0; oy--) {
        // Coordenada fuente en punto fijo (centro de pixel)
        int32_t syf = ((int32_t)oy * 2 + 1) * h * 128 / oh - 128; // *256/2
        if (syf < 0) syf = 0;
        int sy0 = syf >> 8;
        int sy1 = sy0 + 1 < h ? sy0 + 1 : sy0;
        int fy  = syf & 0xFF;
        const lv_color_t* r0 = px + (uint32_t)sy0 * w;
        const lv_color_t* r1 = px + (uint32_t)sy1 * w;

        for (int ox = 0; ox < ow; ox++) {
            int32_t sxf = ((int32_t)ox * 2 + 1) * w * 128 / ow - 128;
            if (sxf < 0) sxf = 0;
            int sx0 = sxf >> 8;
            int sx1 = sx0 + 1 < w ? sx0 + 1 : sx0;
            int fx  = sxf & 0xFF;

            lv_color32_t c00; c00.full = lv_color_to32(r0[sx0]);
            lv_color32_t c01; c01.full = lv_color_to32(r0[sx1]);
            lv_color32_t c10; c10.full = lv_color_to32(r1[sx0]);
            lv_color32_t c11; c11.full = lv_color_to32(r1[sx1]);

            // Interpolar por canal: horizontal en ambas filas, luego vertical
            #define _BL(ch) (uint8_t)(( \
                ( (c00.ch*(256-fx) + c01.ch*fx) * (256-fy) + \
                  (c10.ch*(256-fx) + c11.ch*fx) * fy ) >> 16 ))
            row[ox*3 + 0] = _BL(ch.blue);
            row[ox*3 + 1] = _BL(ch.green);
            row[ox*3 + 2] = _BL(ch.red);
            #undef _BL
        }
        f.write(row, row_bytes);
    }

    free(row);
    f.flush();
    f.close();
    free(buf);
    Serial.printf("[SHOT] guardada %s (%dx%d)\n", path, ow, oh);
    return true;
}

// Llamar en cada vuelta de loop(). Lee el serial buscando el comando "shot".
static void screenshot_poll() {
    static char cmd[8];
    static uint8_t ci = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            cmd[ci] = '\0';
            if      (strcmp(cmd, "shot")  == 0) screenshot_take(1);
            else if (strcmp(cmd, "shot2") == 0) screenshot_take(2);
            else if (strcmp(cmd, "shot3") == 0) screenshot_take(3);
            ci = 0;
        } else if (ci < sizeof(cmd) - 1) {
            cmd[ci++] = c;
        } else {
            ci = 0; // comando demasiado largo — descartar
        }
    }
}
