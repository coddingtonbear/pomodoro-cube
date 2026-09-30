#include "nap.h"

#include <Arduino.h>
#include <driver/ledc.h>
#include <esp_sleep.h>

void Nap::keepPwmThroughSleep(int channel, uint32_t frequency, uint8_t bits) {
  // The 2.x core's channel-to-timer mapping (esp32-hal-ledc.c): eight channels
  // to a speed group, two to a timer. The S3 has only the low-speed group.
  ledc_timer_config_t timer = {};
  timer.speed_mode = LEDC_LOW_SPEED_MODE;
  timer.duty_resolution = (ledc_timer_bit_t)bits;
  timer.timer_num = (ledc_timer_t)((channel / 2) % 4);
  timer.freq_hz = frequency;
  timer.clk_cfg = LEDC_USE_RTC8M_CLK;
  if (ledc_timer_config(&timer) != ESP_OK) {
    Serial.println("[trace] NAP pwm clock not moved; the backlight will not survive a nap");
  }
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_ON);
}

void Nap::sleepFor(uint32_t ms) {
  // Whatever is still in the UART's buffer would be lost with its clock.
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  esp_light_sleep_start();
}
