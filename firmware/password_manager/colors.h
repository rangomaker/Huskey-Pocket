#pragma once
#include <lvgl.h>
#include <math.h>

// Colores del tema — compartidos entre ui_main.h y custom_kb.h
// Se inicializan en _apply_theme() dentro de ui_main.h
lv_color_t C_BG, C_TEXT, C_MUTED, C_BORDER, C_CARD;
lv_color_t C_BTN_BG, C_BTN_TEXT, C_DANGER;
lv_color_t C_ACCENT; // color de acento seleccionable — ver vault_get_accent()
lv_color_t C_ACCENT_TEXT; // texto de contraste sobre botones tipo pill con C_ACCENT (fijo: blanco)

// Paleta de acentos seleccionables (índice persistido en vprefs)
#define ACCENT_COUNT 6
#define ACCENT_AUTO (-1) // "Según tema": usa el contraste invertido (C_BTN_BG/C_BTN_TEXT)
static const uint32_t ACCENT_PALETTE[ACCENT_COUNT] = {
    0x0A84FF, // Azul (default)
    0x30D158, // Verde
    0xFF9F0A, // Naranja
    0xFF375F, // Rosa
    0xBF5AF2, // Morado
    0x64D2FF, // Celeste
};

// Tema: 0=oscuro, 1=claro, 2=personalizado (fondo/tarjeta/acento con Hue+Intensidad)
#define THEME_OSCURO 0
#define THEME_CLARO 1
#define THEME_PERSONALIZADO 2

// Convierte HSV (h:0-360, s/v:0-100) a lv_color_t — usado por el tema
// Personalizado para fondo/tarjeta/acento.
static inline lv_color_t hsv_to_lv_color(int h, int s, int v) {
    float S = s / 100.0f, V = v / 100.0f;
    float C = V * S;
    float Hp = h / 60.0f;
    float X = C * (1.0f - fabsf(fmodf(Hp, 2.0f) - 1.0f));
    float m = V - C;
    float r, g, b;
    if      (h < 60)  { r = C; g = X; b = 0; }
    else if (h < 120) { r = X; g = C; b = 0; }
    else if (h < 180) { r = 0; g = C; b = X; }
    else if (h < 240) { r = 0; g = X; b = C; }
    else if (h < 300) { r = X; g = 0; b = C; }
    else              { r = C; g = 0; b = X; }
    uint8_t R = (uint8_t)((r + m) * 255.0f + 0.5f);
    uint8_t G = (uint8_t)((g + m) * 255.0f + 0.5f);
    uint8_t B = (uint8_t)((b + m) * 255.0f + 0.5f);
    return lv_color_make(R, G, B);
}

// Luminancia perceptual aprox. — para elegir texto blanco/negro de contraste
// sobre un color elegido libremente (tema Personalizado).
static inline bool color_is_light(lv_color_t c) {
    uint32_t hex = lv_color_to32(c) & 0xFFFFFF;
    int r = (hex >> 16) & 0xFF, g = (hex >> 8) & 0xFF, b = hex & 0xFF;
    return (0.299f * r + 0.587f * g + 0.114f * b) > 150.0f;
}
