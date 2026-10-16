#pragma once
#include <Arduino.h>

#define PIN_VIBRO 43

void vibro_begin()                                    {}
void vibro_on()                                       {}
void vibro_off()                                      {}
void vibro_pulse(uint16_t ms = 65, uint8_t i = 255)  {}
void vibro_pulse_error()                              {}
