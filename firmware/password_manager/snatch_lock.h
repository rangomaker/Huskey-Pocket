#pragma once
// =============================================================
//  snatch_lock.h — Bloqueo automático por arrebato (Snatch Lock)
//  IMU QMI8658C — bus Wire compartido con el RTC (SDA=GPIO11, SCL=GPIO10).
//  Lectura por software (polling).
//
//  Detección: si la magnitud de aceleración Y la de giro superan
//  el umbral del nivel configurado, se activa el bloqueo (Deep Sleep).
// =============================================================
#include <Wire.h>

// Forward declarations (vault.h / ui_main.h / power_manager.h)
void vault_lock();
bool vault_is_ready();
bool vault_is_op_busy();
int  vault_get_snatch_level();
void ui_main_prepare_lock();
void power_deep_sleep();

#define QMI_SDA_PIN     11
#define QMI_SCL_PIN     10
#define QMI_ADDR        0x6B   // confirmado por el demo de fábrica Waveshare

#define QMI_REG_WHOAMI  0x00
#define QMI_REG_CTRL1   0x02
#define QMI_REG_CTRL2   0x03
#define QMI_REG_CTRL3   0x04
#define QMI_REG_CTRL5   0x06
#define QMI_REG_CTRL7   0x08
#define QMI_REG_AX_L    0x35   // AX_L..GZ_H — 12 bytes contiguos

static bool _snatch_ready = false;

static bool _qmi_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(QMI_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission(true) == 0;
}

static bool _qmi_read(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(QMI_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)QMI_ADDR, (int)len) != (int)len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

// Llamar UNA vez en setup(), después de rtc_begin() (mismo bus I2C,
// ya inicializado — Wire.begin() es seguro de llamar de nuevo con
// los mismos pines).
bool snatch_lock_init() {
    Wire.begin(QMI_SDA_PIN, QMI_SCL_PIN);

    uint8_t who = 0;
    if (!_qmi_read(QMI_REG_WHOAMI, &who, 1) || who != 0x05) {
        Serial.printf("[SNATCH] QMI8658 no responde (who=0x%02x)\n", who);
        return false;
    }

    _qmi_write(QMI_REG_CTRL1, 0x40); // auto-increment de dirección, 2MHz osc
    _qmi_write(QMI_REG_CTRL2, 0x23); // accel ±8g, ODR 1000Hz normal
    _qmi_write(QMI_REG_CTRL3, 0x63); // giro ±1024dps, ODR 1000Hz normal
    _qmi_write(QMI_REG_CTRL5, 0x00); // sin filtro — respuesta más rápida
    _qmi_write(QMI_REG_CTRL7, 0x43); // reloj alta velocidad, accel+gyro ON

    _snatch_ready = true;
    Serial.println("[SNATCH] QMI8658 listo");
    return true;
}

// Umbrales por nivel: aceleración (g) y giro (°/s). Nivel 0 = desactivado.
struct SnatchThresholds { float accel_g; float gyro_dps; };
static const SnatchThresholds SNATCH_LEVELS[4] = {
    {0.0f, 0.0f},   // 0 = desactivado
    {4.0f, 300.0f}, // 1 = baja
    {3.0f, 200.0f}, // 2 = media
    {2.5f, 150.0f}, // 3 = alta
};

// Evaluación periódica desde loop() (requiere vault desbloqueado y sin operación en curso)
void snatch_lock_tick() {
    if (!_snatch_ready) return;
    int level = vault_get_snatch_level();
    if (level <= 0 || level > 3) return; // 1..3 válidos; fuera de rango, no evaluar
    if (!vault_is_ready()) return;   // solo importa con el vault desbloqueado
    if (vault_is_op_busy()) return;  // no interrumpir una escritura en curso

    uint8_t buf[12];
    if (!_qmi_read(QMI_REG_AX_L, buf, 12)) return;

    int16_t ax = (int16_t)((buf[1]  << 8) | buf[0]);
    int16_t ay = (int16_t)((buf[3]  << 8) | buf[2]);
    int16_t az = (int16_t)((buf[5]  << 8) | buf[4]);
    int16_t gx = (int16_t)((buf[7]  << 8) | buf[6]);
    int16_t gy = (int16_t)((buf[9]  << 8) | buf[8]);
    int16_t gz = (int16_t)((buf[11] << 8) | buf[10]);

    const float ACCEL_SCALE = 8.0f    / 32768.0f; // ±8g   (CTRL2 aFS=010)
    const float GYRO_SCALE  = 1024.0f / 32768.0f; // ±1024dps (CTRL3 gFS=110)

    float axg = ax * ACCEL_SCALE, ayg = ay * ACCEL_SCALE, azg = az * ACCEL_SCALE;
    float gxd = gx * GYRO_SCALE,  gyd = gy * GYRO_SCALE,  gzd = gz * GYRO_SCALE;

    float accel_mag = sqrtf(axg * axg + ayg * ayg + azg * azg);
    float gyro_mag  = sqrtf(gxd * gxd + gyd * gyd + gzd * gzd);

    const SnatchThresholds& t = SNATCH_LEVELS[level];
    if (accel_mag > t.accel_g && gyro_mag > t.gyro_dps) {
        Serial.printf("[SNATCH] Disparo — accel=%.1fg giro=%.0fdps (nivel=%d)\n",
                      accel_mag, gyro_mag, level);
        ui_main_prepare_lock();
        vault_lock();
        power_deep_sleep();
    }
}
