#pragma once
// =============================================================
//  fonts.h — Fuentes custom de KeyVault
//  Inter Medium + ExtraBold + Material Symbols Rounded Filled
// =============================================================

#ifdef __cplusplus
extern "C" {
#endif

// ── Declaraciones ─────────────────────────────────────────────
extern const lv_font_t inter_m_12;
extern const lv_font_t inter_m_14;
extern const lv_font_t inter_m_16;
extern const lv_font_t inter_eb_16;
extern const lv_font_t inter_eb_20;
extern const lv_font_t material_16;
extern const lv_font_t material_16_extra;

#ifdef __cplusplus
}
#endif

// ── Aliases convenientes ──────────────────────────────────────
#define FONT_SM       &inter_m_12    // texto pequeño, labels
#define FONT_MD       &inter_m_14    // texto principal
#define FONT_LG       &inter_m_16    // texto medio
#define FONT_HDR      &inter_eb_16   // headers, títulos
#define FONT_HDR_LG   &inter_eb_20   // títulos grandes
#define FONT_ICON     &material_16        // íconos Material Symbols (base)
#define FONT_ICON_EX  &material_16_extra  // íconos Material Symbols (extra)

// ── Codepoints de íconos Material Symbols ─────────────────────
#define ICO_BACKSPACE     "\xee\x85\x8a"  // e14a
#define ICO_CAPSLOCK      "\xEE\x8C\x98"  // e318
#define ICO_CHEVRON_LEFT  "\xEE\x97\x8B"  // e5cb
#define ICO_CHEVRON_RIGHT "\xEE\x97\x8C"  // e5cc
#define ICO_ARROW_BACK    "\xEE\x97\x84"  // e5c4
#define ICO_CLOSE         "\xEE\x97\x8D"  // e5cd
#define ICO_MENU          "\xEE\x97\x92"  // e5d2
#define ICO_HOME          "\xEE\xA2\x8A"  // e88a
#define ICO_ADD           "\xEE\x85\x85"  // e145
#define ICO_EDIT          "\xEE\x8F\x89"  // e3c9
#define ICO_DELETE        "\xEE\xA1\xB2"  // e872
#define ICO_CHECK         "\xEE\x97\x8A"  // e5ca
#define ICO_REFRESH       "\xEE\x97\x95"  // e5d5
#define ICO_SORT          "\xEE\x85\xA4"  // e164
#define ICO_SHUFFLE       "\xEE\x81\x83"  // e043
#define ICO_COPY          "\xEE\x85\x8D"  // e14d
#define ICO_SEARCH        "\xEE\xA2\xB6"  // e8b6
#define ICO_SETTINGS      "\xEE\xA2\xB8"  // e8b8
#define ICO_STAR          "\xEE\xA0\xB8"  // e838
#define ICO_FOLDER        "\xEE\x8B\x87"  // e2c7
#define ICO_VISIBILITY    "\xEE\xA3\xB4"  // e8f4
#define ICO_VISIBILITY_OFF "\xEE\xA3\xB5" // e8f5
#define ICO_LOCK          "\xEE\xA2\x97"  // e897
#define ICO_FAVORITE      "\xEE\xA1\xBD"  // e87d
#define ICO_CATEGORY      "\xEE\x95\xB4"  // e574
#define ICO_INFO          "\xEE\xA2\x8E"  // e88e
#define ICO_WARNING       "\xEE\x80\x82"  // e002
#define ICO_PASSWORD      "\xef\x81\x82"  // f042
#define ICO_KEY           "\xEE\x9C\xBC"  // e73c
#define ICO_WIFI          "\xEE\x98\xBE"  // e63e
#define ICO_NOTE          "\xEE\x99\xAD"  // e66d
#define ICO_NOTE_ADD      "\xEE\xA2\x9C"  // e89c
#define ICO_STICKY_NOTE   "\xef\x87\xbc"  // f1fc
#define ICO_CALENDAR      "\xEE\xA4\xB5"  // e935
#define ICO_SCHEDULE      "\xEE\xA2\xB5"  // e8b5
#define ICO_ALARM         "\xEE\xA1\x95"  // e855
#define ICO_SHIELD        "\xEE\xA7\xA0"  // e9e0
#define ICO_FINGERPRINT   "\xEE\xA4\x8D"  // e90d
#define ICO_PRIVACY       "\xef\x83\x9c"  // f0dc
#define ICO_NO_ENCRYPT    "\xEE\x99\x81"  // e641
#define ICO_BATTERY_FULL  "\xEE\x86\xA4"  // e1a4
#define ICO_BATTERY_CHARGE "\xEE\x86\xA3" // e1a3
#define ICO_BATTERY_ALERT "\xEE\x86\x9C"  // e19c
#define ICO_BLUETOOTH     "\xEE\x86\xA7"  // e1a7
#define ICO_BT_CONNECTED  "\xEE\x86\xA8"  // e1a8
#define ICO_USB           "\xEE\x87\xA0"  // e1e0
#define ICO_CASINO        "\xEE\xAD\x80"  // eb40
#define ICO_AUTORENEW     "\xEE\xA1\xA3"  // e863
#define ICO_DARK_MODE     "\xEE\x94\x9C"  // e51c
#define ICO_LIGHT_MODE    "\xEE\x94\x98"  // e518
#define ICO_LANGUAGE      "\xEE\xA2\x94"  // e894
#define ICO_KEYBOARD      "\xEE\x8C\x92"  // e312
#define ICO_QR_CODE       "\xEE\xBD\xAB"  // ef6b
#define ICO_DOWNLOAD      "\xef\x82\x90"  // f090
#define ICO_UPLOAD        "\xef\x82\x9b"  // f09b
#define ICO_SYNC          "\xEE\x98\xA7"  // e627
#define ICO_PIN           "\xef\x81\x85"  // f045
#define ICO_LOCK_CLOCK    "\xEE\xBD\x97"  // ef57

// ── Íconos extra (material_16_extra.c) ───────────────────────
#define ICO_SELECT_ALL         "\xEE\x85\xA2"  // e162
#define ICO_WIDGET             "\xEE\x86\xBD"  // e1bd
#define ICO_APPS               "\xEE\x97\x83"  // e5c3
#define ICO_PERSON             "\xEE\x9F\xBD"  // e7fd
#define ICO_CHECK_CIRCLE       "\xEE\xA1\xAC"  // e86c
#define ICO_LIST               "\xEE\xA2\x96"  // e896
#define ICO_GRID_VIEW          "\xEE\xA2\xB0"  // e8b0
#define ICO_SWAP_VERT          "\xEE\xA3\x96"  // e8d6
#define ICO_SWITCH_ACCOUNT     "\xEE\xA7\xAD"  // e9ed
#define ICO_FILE_SAVE          "\xEF\x85\xBF"  // f17f
#define ICO_FILE_EXPORT        "\xEF\x8E\xB2"  // f3b2
#define ICO_TILE_SMALL         "\xEF\x8F\x81"  // f3c1
#define ICO_EVENT_LIST         "\xEF\x9A\x83"  // f683

// Fuente extra — declaración externa
LV_FONT_DECLARE(material_16_extra)

// FONT_ICON usa la fuente combinada — en código usar:
// lv_obj_set_style_text_font(obj, &material_16_extra, 0) para íconos extra
