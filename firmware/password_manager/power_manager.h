#pragma once
// =============================================================
//  power_manager.h — Control de botón físico (GPIO6) y MOSFET de alimentación (GPIO7)
//  Waveshare ESP32-S3-Touch-LCD-2.8
//
//  - Tap corto (< 1s): Deep Sleep con wake-up por GPIO6
//  - Hold medio (2.5s): Menú Apagar / Reiniciar / Bloqueo
//  - Hold forzado (10s): Reinicio hardware forzado
//  - Apagado: GPIO7 a LOW corta el paso del MOSFET
// =============================================================
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h> // gpio_hold_en/dis — sostener GPIO7 durante deep sleep

void ble_kb_end(); // definida en ble_keyboard_manager.h
bool vault_is_op_busy(); // definida en vault.h — operación import/export en curso

// Declaradas solo en sus .cpp (no en sus .h) — necesarias aquí para
// la secuencia de bajo consumo antes de Deep Sleep.
void LCD_WriteCommand(uint8_t Cmd); // definida en Display_ST7789.cpp
bool Touch_I2C_Write(uint8_t Driver_addr, uint16_t Reg_addr, const uint8_t *Reg_data, uint32_t Length); // definida en Touch_CST328.cpp

#define PWR_KEY_PIN      6   // entrada — lectura del botón físico
#define PWR_CTRL_PIN     7   // salida — sostiene/corta el MOSFET de alimentación

#define PWR_TAP_MAX_MS       1000   // hasta aquí cuenta como "tap corto"
#define PWR_HOLD_MENU_MS     2500   // a partir de aquí abre el menú
#define PWR_HOLD_FORCE_MS   10000   // a partir de aquí reinicio forzado, sin UI

// Callback hacia la UI — se define en power_ui.h.
// show_lock=true  → muestra opción de Bloqueo (dispositivo desbloqueado)
// show_lock=false → oculta opción de Bloqueo (lock/pin screen activos)
void power_show_menu(bool show_lock); // forward decl — implementado en power_ui.h

// Estado de bloqueo — actualizar desde lock_screen/pin_screen
// cuando el dispositivo está bloqueado, para que el menú de
// power no muestre la opción de Bloqueo en ese contexto.
static bool _pwr_device_locked = false;
void power_set_locked(bool locked) { _pwr_device_locked = locked; }

static bool     _pwr_btn_was_down   = false;
static uint32_t _pwr_press_start_ms = 0;
static bool     _pwr_menu_shown     = false;
static bool     _pwr_force_done     = false;
static bool     _pwr_boot_release_done = false; // ignora el soltado inicial del boot

// Llamar UNA vez en setup(), lo antes posible — antes de cualquier
// inicialización pesada (display, SD, etc.) para minimizar la
// ventana en la que el usuario podría soltar el botón y la placa
// se apague sola por falta de sostenimiento.
void power_begin() {
    // Liberar hold previo si despertó de deep sleep
    gpio_hold_dis((gpio_num_t)PWR_CTRL_PIN);
    pinMode(PWR_CTRL_PIN, OUTPUT);
    digitalWrite(PWR_CTRL_PIN, HIGH); // Sostiene la alimentación vía MOSFET
    pinMode(PWR_KEY_PIN, INPUT);
    Serial.println("[PWR] Alimentación sostenida (GPIO7=HIGH)");
}

// Corte de alimentación física (MOSFET abierto)
void power_shutdown() {
    Serial.println("[PWR] Apagando (corte de GPIO7)...");
    delay(50);
    digitalWrite(PWR_CTRL_PIN, LOW);
    while (true) { delay(1000); }
}

void power_restart() {
    Serial.println("[PWR] Reiniciando...");
    delay(50);
    ESP.restart();
}

// Deep Sleep con wake-up por botón físico (GPIO6)
void power_deep_sleep() {
    if (vault_is_op_busy()) {
        Serial.println("[PWR] Deep Sleep pospuesto: operación del vault en curso");
        return;
    }
    Serial.println("[PWR] Entrando a Deep Sleep...");

    // Suspender periféricos para minimizar consumo
    Set_Backlight(0);
    LCD_WriteCommand(0x10); // ST7789 SLPIN
    delay(5);
    { uint8_t dummy = 0; Touch_I2C_Write(CST328_ADDR, HYN_REG_MUT_DEEP_SLEEP_MODE, &dummy, 0); }
    sd_end();

    ble_kb_end();
    delay(50);
    // Esperar a que se suelte el botón antes de dormir
    while (digitalRead(PWR_KEY_PIN) == LOW) delay(10);
    delay(50);

    // Retener GPIO7 en HIGH durante deep sleep para mantener el MOSFET activo
    gpio_hold_en((gpio_num_t)PWR_CTRL_PIN);
    gpio_deep_sleep_hold_en();

    esp_sleep_enable_ext0_wakeup((gpio_num_t)PWR_KEY_PIN, 0);
    esp_deep_sleep_start();
}

// Llamar en cada vuelta del loop() principal. La detección de hold
// forzado (10s) vive aquí, leyendo el pin directo con millis(), NO
// dentro de un callback de LVGL — así funciona incluso si la UI
// está congelada/colgada.
void power_tick() {
    bool down = (digitalRead(PWR_KEY_PIN) == LOW); // activo en bajo, ver esquemático

    // Al arrancar el botón ya estaba presionado (el usuario lo sostuvo para encender).
    // El primer soltado que detectamos es ese — ignorarlo para que el dispositivo
    // no entre en deep sleep justo cuando el usuario suelta el botón de encendido.
    if (!_pwr_boot_release_done) {
        if (!down) _pwr_boot_release_done = true; // esperamos a que lo suelte por primera vez
        _pwr_btn_was_down = down;
        return;
    }

    if (down && !_pwr_btn_was_down) {
        // Flanco de bajada — empieza a contar
        _pwr_press_start_ms = millis();
        _pwr_menu_shown      = false;
        _pwr_force_done      = false;
    }

    if (down) {
        uint32_t held = millis() - _pwr_press_start_ms;

        // Hold forzado — dispara UNA sola vez, sin esperar a soltar,
        // sin pasar por UI/menú. Funciona aunque LVGL esté trabado.
        if (held >= PWR_HOLD_FORCE_MS && !_pwr_force_done) {
            _pwr_force_done = true;
            power_restart();
            return; // no llega — restart() no regresa
        }

        // Hold medio — abre el menú UNA sola vez mientras se mantiene
        if (held >= PWR_HOLD_MENU_MS && !_pwr_menu_shown) {
            _pwr_menu_shown = true;
            power_show_menu(!_pwr_device_locked);
        }
    } else if (_pwr_btn_was_down) {
        // Flanco de subida — el usuario soltó el botón
        uint32_t held = millis() - _pwr_press_start_ms;
        if (held < PWR_TAP_MAX_MS) {
            // Tap corto — solo si no se llegó a abrir el menú ni el
            // forzado (evita dormir justo al soltar después de un hold)
            power_deep_sleep();
        }
        // Si fue hold medio (se mostró el menú), soltar el botón NO
        // hace nada por sí solo — la decisión la toma el usuario
        // dentro del menú (Apagar/Reiniciar/Cancelar).
    }

    _pwr_btn_was_down = down;
}
