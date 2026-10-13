// Stands in for nap.cpp: the host has no light sleep to go into and no PWM
// clock to keep, so a nap is only the wait
// -- and the timer wake it arms, which outlives it.
#include "nap.h"

#include "Arduino.h"

void Nap::keepPwmThroughSleep(int, uint32_t, uint8_t) {}

void Nap::sleepFor(uint32_t ms) {
  // Armed as the device arms it, and left armed as the device leaves it.
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  delay(ms);
}
