// Stands in for nap.cpp: the host has no light sleep to go into and no PWM
// clock to keep, so a nap is only the wait.
#include "nap.h"

#include "Arduino.h"

void Nap::keepPwmThroughSleep(int, uint32_t, uint8_t) {}

void Nap::sleepFor(uint32_t ms) {
  delay(ms);
}
