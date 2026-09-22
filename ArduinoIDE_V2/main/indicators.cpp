#include "indicators.h"

#include "consts.h"

namespace {

// Blend one channel of `from` towards `to`; ratio runs 0 (all from) to 255.
uint32_t mixChannel(uint32_t from, uint32_t to, int shift, int ratio) {
  const int a = (int)((from >> shift) & 0xFF);
  const int b = (int)((to >> shift) & 0xFF);
  return (uint32_t)(a + ((b - a) * ratio) / 255) << shift;
}

uint32_t mix(uint32_t from, uint32_t to, int ratio) {
  return mixChannel(from, to, 16, ratio) | mixChannel(from, to, 8, ratio) |
         mixChannel(from, to, 0, ratio);
}

}  // namespace

int Indicators::remainingPercent(int remSeconds, int selSeconds) {
  if (selSeconds <= 0 || remSeconds <= 0) return 0;
  if (remSeconds >= selSeconds) return 100;
  return (remSeconds * 100) / selSeconds;
}

uint32_t Indicators::arcColor(int remainingPercent) {
  if (remainingPercent >= 100) return ARC_COLOR_FULL;

  if (remainingPercent >= ARC_MID_PERCENT) {
    const int ratio = ((remainingPercent - ARC_MID_PERCENT) * 255) / (100 - ARC_MID_PERCENT);
    return mix(ARC_COLOR_MID, ARC_COLOR_FULL, ratio);
  }

  if (remainingPercent >= ARC_LOW_PERCENT) {
    const int ratio = ((remainingPercent - ARC_LOW_PERCENT) * 255) / (ARC_MID_PERCENT - ARC_LOW_PERCENT);
    return mix(ARC_COLOR_LOW, ARC_COLOR_MID, ratio);
  }

  return ARC_COLOR_LOW;
}

bool Indicators::showLowBattery(float batteryVoltage) {
  return batteryVoltage < LOW_BATTERY_VOLTAGE;
}
