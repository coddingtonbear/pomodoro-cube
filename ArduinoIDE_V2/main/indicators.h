#pragma once
#include <stdint.h>

// Pure display logic, kept free of LVGL so it can be unit tested on the host.
// display.cpp does the drawing; these decide what should be drawn.
namespace Indicators {

// How much of the selected timer is left, 0-100. Clamped, and safe when
// selSeconds is zero.
int remainingPercent(int remSeconds, int selSeconds);

// Arc colour as 0xRRGGBB, interpolated between the stops in consts.h: full at
// 100%, mid at ARC_MID_PERCENT, low at and below ARC_LOW_PERCENT.
uint32_t arcColor(int remainingPercent);

// Whether the low-battery warning should be on screen, given the smoothed
// pack voltage in volts.
bool showLowBattery(float batteryVoltage);

}  // namespace Indicators
