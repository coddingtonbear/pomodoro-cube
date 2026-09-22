// Stubs for the ESP32 RTC GPIO calls made on the way into deep sleep.
#pragma once

#include <Arduino.h>

inline void rtc_gpio_pullup_dis(gpio_num_t pin) { (void)pin; }
inline void rtc_gpio_pulldown_en(gpio_num_t pin) { (void)pin; }
inline void rtc_gpio_pullup_en(gpio_num_t pin) { (void)pin; }
inline void rtc_gpio_pulldown_dis(gpio_num_t pin) { (void)pin; }
