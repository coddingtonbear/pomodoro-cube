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

// Shared by both arcs: full at 100%, mid at ARC_MID_PERCENT, low at and below
// ARC_LOW_PERCENT, interpolated in between.
uint32_t ramp(int percent, uint32_t full, uint32_t mid, uint32_t low) {
  if (percent >= 100) return full;

  if (percent >= ARC_MID_PERCENT) {
    const int ratio = ((percent - ARC_MID_PERCENT) * 255) / (100 - ARC_MID_PERCENT);
    return mix(mid, full, ratio);
  }

  if (percent >= ARC_LOW_PERCENT) {
    const int ratio = ((percent - ARC_LOW_PERCENT) * 255) / (ARC_MID_PERCENT - ARC_LOW_PERCENT);
    return mix(low, mid, ratio);
  }

  return low;
}

}  // namespace

int Indicators::remainingPercent(int remSeconds, int selSeconds) {
  if (selSeconds <= 0 || remSeconds <= 0) return 0;
  if (remSeconds >= selSeconds) return 100;
  return (remSeconds * 100) / selSeconds;
}

uint32_t Indicators::arcColor(int remainingPercent) {
  return ramp(remainingPercent, ARC_COLOR_FULL, ARC_COLOR_MID, ARC_COLOR_LOW);
}

int Indicators::lapPercent(int elapsedSeconds) {
  if (elapsedSeconds <= 0) return 0;
  return ((elapsedSeconds % FLOW_LAP_SECONDS) * 100) / FLOW_LAP_SECONDS;
}

uint32_t Indicators::flowArcColor(int remainingPercent) {
  return ramp(remainingPercent, FLOW_ARC_COLOR_FULL, FLOW_ARC_COLOR_MID, FLOW_ARC_COLOR_LOW);
}

Indicators::Palette Indicators::palette(int rampAt, bool flow, bool dim) {
  const uint32_t ramped = flow ? flowArcColor(rampAt) : arcColor(rampAt);

  if (!dim) {
    if (flow) {
      return {FLOW_BG_COLOR, COUNTDOWN_COLOR_FLOW, ramped, FLOW_ARC_TRACK_COLOR,
              LOW_BATTERY_COLOR};
    }
    return {SCREEN_BG_COLOR, COUNTDOWN_COLOR, ramped, ARC_TRACK_COLOR, LOW_BATTERY_COLOR};
  }

  // The swap is literal: whatever the background was becomes what is drawn on
  // it. No new colours are needed, and the pairing stays contrasty by
  // construction -- flow's ramp is the darker one and was already the mode
  // drawn on white, so it gets white.
  const uint32_t foreground = flow ? FLOW_BG_COLOR : SCREEN_BG_COLOR;

  // Track hidden. It would be a second tone on a field that is already carrying
  // the reading, and what is left -- the arc alone -- is the part worth seeing
  // at a glance. The full ring is one glance away in bright mode when the exact
  // fraction matters.
  return {ramped, foreground, foreground, ramped, foreground};
}

Indicators::Palette Indicators::alertPalette(bool inverted) {
  // ARC_COLOR_LOW rather than a red of its own: it is the colour an expiring
  // timer has been shading towards for the last quarter of its run, so the
  // alarm arrives as the end of that journey rather than as a new idea.
  const uint32_t face = inverted ? ARC_COLOR_LOW : SCREEN_BG_COLOR;
  const uint32_t drawn = inverted ? SCREEN_BG_COLOR : ARC_COLOR_LOW;

  // Track set to the face so the ring leaves no groove behind even if something
  // fails to hide the arc itself. Battery takes the drawn colour for the same
  // reason it does when dim: red on red is nothing at all.
  return {face, drawn, face, face, drawn};
}

Indicators::ClockFields Indicators::clockFields(int seconds) {
  if (seconds < 0) seconds = 0;
  if (seconds >= 3600) return {seconds / 3600, (seconds % 3600) / 60, true};
  return {seconds / 60, seconds % 60, false};
}

bool Indicators::showLowBattery(float batteryVoltage) {
  return batteryVoltage < LOW_BATTERY_VOLTAGE;
}
