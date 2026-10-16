#pragma once
// =============================================================
//  rtc_manager.h — RTC dedicado PCF85063ATL (I2C, 0x51)
//  Waveshare ESP32-S3-Touch-LCD-2.8
//  Bus: Wire (SDA=GPIO11, SCL=GPIO10) — compartido con IMU QMI8658
//  Con batería propia (conector J7 en la placa) — mantiene la hora
//  aunque el dispositivo se quede sin energía principal.
// =============================================================
#include <Wire.h>

#define PCF85063_ADDR     0x51
#define PCF85063_SDA_PIN  11
#define PCF85063_SCL_PIN  10

// Registros PCF85063
#define PCF_REG_CTRL1     0x00
#define PCF_REG_CTRL2     0x01
#define PCF_REG_SECONDS   0x04  // bits[6:0] = segundos BCD, bit7 = OS (clock integrity)
#define PCF_REG_MINUTES   0x05
#define PCF_REG_HOURS     0x06
#define PCF_REG_DAYS      0x07
#define PCF_REG_WEEKDAYS  0x08  // 0=domingo .. 6=sábado
#define PCF_REG_MONTHS    0x09  // 1-12
#define PCF_REG_YEARS     0x0A  // 0-99 (offset 1970, igual que demo Waveshare)

// ── Helpers BCD ───────────────────────────────────────────────
static inline uint8_t _bcd2dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static inline uint8_t _dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

static bool _rtc_was_set = false;
static bool _rtc_i2c_ok  = false;

static uint8_t _pcf_read(uint8_t reg) {
    Wire.beginTransmission(PCF85063_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0;
    Wire.requestFrom(PCF85063_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
}

static void _pcf_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(PCF85063_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission(true);
}

// Llama una vez en setup() — Wire ya debe estar inicializado
// (Touch_Init() lo inicia en Wire1, aquí usamos Wire normal)
void rtc_begin() {
    // Wire puede estar ya iniciado por el IMU — begin() es seguro
    // de llamar varias veces con los mismos pines en Arduino-ESP32
    Wire.begin(PCF85063_SDA_PIN, PCF85063_SCL_PIN);

    // Verificar presencia del chip
    Wire.beginTransmission(PCF85063_ADDR);
    _rtc_i2c_ok = (Wire.endTransmission(true) == 0);
    if (!_rtc_i2c_ok) {
        Serial.println("[RTC] PCF85063 no encontrado");
        return;
    }

    // Limpiar STOP bit (bit5 de CTRL1) por si quedó en modo detenido
    uint8_t ctrl1 = _pcf_read(PCF_REG_CTRL1);
    ctrl1 &= ~(1 << 5); // STOP = 0
    _pcf_write(PCF_REG_CTRL1, ctrl1);

    // Verificar OS bit (clock integrity loss) — si está en 1, el RTC
    // perdió la hora (primera vez, batería muerta, etc.)
    uint8_t sec_reg = _pcf_read(PCF_REG_SECONDS);
    _rtc_was_set = !(sec_reg & 0x80); // OS=0 → hora válida
    Serial.printf("[RTC] PCF85063 OK — hora %s\n", _rtc_was_set ? "válida" : "no ajustada");
}

// ── API pública ───────────────────────────────────────────────

void rtc_set(int year, int month, int day, int hour, int minute, int second = 0) {
    if (!_rtc_i2c_ok) return;
    // Validar rangos antes de escribir
    if (second < 0 || second > 59) second = 0;
    if (minute < 0 || minute > 59) minute = 0;
    if (hour   < 0 || hour   > 23) hour   = 0;
    if (day    < 1 || day    > 31) day    = 1;
    if (month  < 1 || month  > 12) month  = 1;
    if (year < 1970 || year > 2069) year  = 2024;
    // Calcular día de semana (Zeller simplificado)
    // Necesario porque PCF85063 lo almacena pero no lo calcula
    int y = year, m = month, d = day;
    if (m < 3) { m += 12; y--; }
    int dow = (d + (13*(m+1)/5) + y + y/4 - y/100 + y/400) % 7;
    // Zeller da 0=sábado,1=domingo..6=viernes → convertir a 0=domingo
    dow = (dow + 6) % 7;

    _pcf_write(PCF_REG_SECONDS,  _dec2bcd(second) & 0x7F); // escribe OS=0
    _pcf_write(PCF_REG_MINUTES,  _dec2bcd(minute));
    _pcf_write(PCF_REG_HOURS,    _dec2bcd(hour));
    _pcf_write(PCF_REG_DAYS,     _dec2bcd(day));
    _pcf_write(PCF_REG_WEEKDAYS, (uint8_t)dow);
    _pcf_write(PCF_REG_MONTHS,   _dec2bcd(month));
    _pcf_write(PCF_REG_YEARS,    _dec2bcd(year - 1970));
    _rtc_was_set = true;
}

bool rtc_is_set() { return _rtc_was_set; }

void rtc_get_time_str(char* out, size_t out_len) {
    if (!_rtc_i2c_ok) { snprintf(out, out_len, "--:--"); return; }
    uint8_t h = _bcd2dec(_pcf_read(PCF_REG_HOURS)   & 0x3F);
    uint8_t m = _bcd2dec(_pcf_read(PCF_REG_MINUTES)  & 0x7F);
    if (h > 23 || m > 59) { snprintf(out, out_len, "--:--"); return; }
    if (vault_get_time_24h()) {
        snprintf(out, out_len, "%02d:%02d", h, m);
    } else {
        uint8_t h12 = h % 12; if (h12 == 0) h12 = 12;
        snprintf(out, out_len, "%d:%02d", h12, m);
    }
}

void rtc_get_date_str(char* out, size_t out_len) {
    if (!_rtc_i2c_ok) { snprintf(out, out_len, "----"); return; }
    int y = _bcd2dec(_pcf_read(PCF_REG_YEARS))   + 1970;
    int mo= _bcd2dec(_pcf_read(PCF_REG_MONTHS)  & 0x1F);
    int d = _bcd2dec(_pcf_read(PCF_REG_DAYS)    & 0x3F);
    snprintf(out, out_len, "%04d-%02d-%02d", y, mo, d);
}

int rtc_get_year()    { return _bcd2dec(_pcf_read(PCF_REG_YEARS)) + 1970; }
int rtc_get_month()   { return _bcd2dec(_pcf_read(PCF_REG_MONTHS)   & 0x1F); } // 1-12
int rtc_get_day()     { return _bcd2dec(_pcf_read(PCF_REG_DAYS)     & 0x3F); }
int rtc_get_hour()    { return _bcd2dec(_pcf_read(PCF_REG_HOURS)    & 0x3F); }
int rtc_get_minute()  { return _bcd2dec(_pcf_read(PCF_REG_MINUTES)  & 0x7F); }
int rtc_get_second()  { return _bcd2dec(_pcf_read(PCF_REG_SECONDS)  & 0x7F); }
int rtc_get_weekday() { return _bcd2dec(_pcf_read(PCF_REG_WEEKDAYS) & 0x07); } // 0=domingo

uint32_t rtc_get_epoch() {
    // Epoch aproximado — sin librería time.h completa
    // Sirve para timestamps de vault, no para criptografía
    struct tm t = {};
    t.tm_year = rtc_get_year() - 1900;
    t.tm_mon  = rtc_get_month() - 1;
    t.tm_mday = rtc_get_day();
    t.tm_hour = rtc_get_hour();
    t.tm_min  = rtc_get_minute();
    t.tm_sec  = rtc_get_second();
    return (uint32_t)mktime(&t);
}
