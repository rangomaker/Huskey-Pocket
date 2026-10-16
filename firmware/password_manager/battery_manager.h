#pragma once
// =============================================================
//  battery_manager.h — Gestión y lectura de batería LiPo (GPIO8)
//  Waveshare ESP32-S3-Touch-LCD-2.8.
//
//  - Filtrado con doble EMA (rápida para detección de carga, lenta para UI).
//  - Detección de carga por umbral/tendencia de voltaje.
//  - Compensación de salto de tensión (caída IR) durante transiciones de carga.
// =============================================================
#include <Arduino.h>

#define BATT_ADC_PIN        8       // GPIO8 (divisor resistivo)
#define BATT_REAL_FACTOR    3.004f  // Factor de calibración (4140mV / 1378mV)

#define BATT_HIST_LEN        15   // ~15 segundos de historial (1 lectura/seg)
#define BATT_RISE_ENTER_MV   18   // sube esto o más en esa ventana -> "empezó a cargar"
#define BATT_DROP_EXIT_MV    25   // cae esto desde su pico mientras carga -> "se desconectó"
#define BATT_SAMPLES           5  // promediar para reducir ruido del ADC dentro de una lectura
#define BATT_JUMP_DECAY_MS  90000 // tiempo en que se diluye del todo el salto de carga

static bool  _batt_ready     = false;
static bool  _batt_charging  = false; // estado persistente — no solo "está subiendo ahora"
static int   _batt_charge_peak_mv = -1; // pico alcanzado durante la carga actual

static float _batt_ema_mv      = -1; // EMA rápida — para detectar inicio/fin de carga
static float _batt_ema_slow_mv = -1; // EMA lenta — SOLO para el % mostrado en pantalla

static int      _batt_history[BATT_HIST_LEN] = {0};
static int      _batt_hist_idx   = 0;
static int      _batt_hist_count = 0;

static float    _batt_shown_mv      = -1; // último mV "mostrado" (post-compensación) — ancla
static float    _batt_jump_mv       = 0;  // salto a diluir tras cualquier transición de carga
static uint32_t _batt_jump_start_ms = 0;
static bool     _batt_was_charging  = false;

static bool battery_init() {
    if (_batt_ready) return true;
    analogReadResolution(12); // igual que el demo oficial
    _batt_ready = true;
    Serial.println("[BATT] Inicializado (GPIO8, metodo demo oficial)");
    return true;
}

// Voltaje real de la batería en mV, método del demo oficial
// (analogReadMilliVolts x3 / offset), promediado para reducir
// ruido dentro de una misma lectura. Retorna -1 si falla.
static int _battery_read_instant_mv() {
    if (!_batt_ready && !battery_init()) return -1;

    float sum = 0;
    for (int i = 0; i < BATT_SAMPLES; i++) {
        int raw_mv = analogReadMilliVolts(BATT_ADC_PIN);
        sum += (float)raw_mv * BATT_REAL_FACTOR;
        delay(2);
    }
    return (int)(sum / BATT_SAMPLES);
}

// Lee + actualiza las dos EMAs. Devuelve la EMA rápida (para
// detección de carga); el % mostrado usa _batt_ema_slow_mv aparte.
static int battery_read_mv() {
    int instant_mv = _battery_read_instant_mv();
    if (instant_mv < 0) return (_batt_ema_mv < 0) ? -1 : (int)_batt_ema_mv;

    if (_batt_ema_mv < 0) _batt_ema_mv = instant_mv;
    else _batt_ema_mv = _batt_ema_mv * 0.7f + instant_mv * 0.3f; // ventana ~3-4 lecturas

    if (_batt_ema_slow_mv < 0) _batt_ema_slow_mv = instant_mv;
    else _batt_ema_slow_mv = _batt_ema_slow_mv * 0.95f + instant_mv * 0.05f; // ventana ~20 lecturas

    return (int)_batt_ema_mv;
}

// Dos fases:
// 1) Detecta el INICIO de carga por tendencia (sube de forma sostenida
//    comparado con hace ~15s — una LiPo en descarga normal casi no
//    cambia en esa ventana).
// 2) Una vez detectada, se queda en "cargando" mientras el voltaje no
//    caiga de forma notoria desde su pico — así no se apaga solo
//    porque el voltaje se estabilizó en su nuevo nivel.
static bool _battery_update_charging(int mv) {
    if (mv < 0) return _batt_charging; // lectura inválida — no cambiar el estado actual

    int compare_to = _batt_history[_batt_hist_idx]; // valor de hace ~15 lecturas
    bool have_history = _batt_hist_count >= BATT_HIST_LEN;

    _batt_history[_batt_hist_idx] = mv;
    _batt_hist_idx = (_batt_hist_idx + 1) % BATT_HIST_LEN;
    if (_batt_hist_count < BATT_HIST_LEN) _batt_hist_count++;

    if (!_batt_charging) {
        if (have_history && (mv - compare_to) >= BATT_RISE_ENTER_MV) {
            _batt_charging = true;
            _batt_charge_peak_mv = mv;
        }
    } else {
        if (mv > _batt_charge_peak_mv) _batt_charge_peak_mv = mv;
        if (_batt_charge_peak_mv - mv >= BATT_DROP_EXIT_MV) {
            _batt_charging = false;
            _batt_charge_peak_mv = -1;
        }
    }
    return _batt_charging;
}

static int _battery_mv_to_percent(int mv) {
    if (mv < 0) return -1;
    const int FULL_MV = 4140; // Voltaje máximo calibrado
    int capped = (mv > FULL_MV) ? FULL_MV : mv;
    if (capped <= 3000) return 0;          // Voltaje de corte mínimo (~3.0V)
    if (capped >= FULL_MV) return 100;
    return (capped - 3000) * 100 / (FULL_MV - 3000);
}

// API pública: lectura y estado para la UI
void battery_get_status(int* out_percent, bool* out_charging, int* out_mv = nullptr) {
    int mv = battery_read_mv();
    bool charging = _battery_update_charging(mv);

    float raw_target_mv = _batt_ema_slow_mv;
    if (_batt_shown_mv < 0) _batt_shown_mv = raw_target_mv;

    if (charging != _batt_was_charging) {
        // Compensación temporal del salto de tensión durante la transición de carga
        _batt_jump_mv = raw_target_mv - _batt_shown_mv;
        _batt_jump_start_ms = millis();
    }
    _batt_was_charging = charging;

    uint32_t elapsed = millis() - _batt_jump_start_ms;
    float frac = (elapsed >= BATT_JUMP_DECAY_MS) ? 0.0f : (1.0f - (float)elapsed / BATT_JUMP_DECAY_MS);
    float compensated_mv = raw_target_mv - _batt_jump_mv * frac;
    _batt_shown_mv = compensated_mv;

    int pct = _battery_mv_to_percent((int)compensated_mv);
    if (out_percent)  *out_percent  = (pct < 0) ? 0 : pct;
    if (out_charging) *out_charging = charging;
    if (out_mv)       *out_mv       = mv;
}
