#include "indicators.h"

#include "consts.h"

#include <math.h>
#include <stdio.h>

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

// Three stops: `full` at 100%, `mid` at ARC_MID_PERCENT, `low` at and below
// ARC_LOW_PERCENT, interpolated in between.
uint32_t threeStop(int percent, uint32_t full, uint32_t mid, uint32_t low) {
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

// Two stops, blended straight across: `full` at 100%, `low` at and below
// ARC_LOW_PERCENT, so it lands on its last colour when the three-stop ramps
// do.
uint32_t twoStop(int percent, uint32_t full, uint32_t low) {
  if (percent >= 100) return full;
  if (percent <= ARC_LOW_PERCENT) return low;
  return mix(low, full, ((percent - ARC_LOW_PERCENT) * 255) / (100 - ARC_LOW_PERCENT));
}

}  // namespace

int Indicators::remainingPercent(int remSeconds, int selSeconds) {
  if (selSeconds <= 0 || remSeconds <= 0) return 0;
  if (remSeconds >= selSeconds) return 100;
  return (remSeconds * 100) / selSeconds;
}

int Indicators::litSlices(int amount, int whole) {
  if (whole <= 0 || amount <= 0) return 0;
  if (amount >= whole) return RING_SLICES;
  return (amount * RING_SLICES + whole - 1) / whole;
}

Indicators::SliceEnds Indicators::sliceEnds(int index, int rotationDegrees) {
  // Pixel indices run 0-239, so the panel's middle falls between 119 and 120.
  constexpr float kCentre = 119.5f;
  // The strokes' round ends reach half their width past where they stop.
  constexpr float kReach = RING_BAND_PX / 2.0f - RING_SLICE_WIDTH_PX / 2.0f;
  constexpr float kDegrees = 3.14159265f / 180.0f;

  const float degrees = RING_START_DEGREES + rotationDegrees +
                        (index + 0.5f) * RING_SWEEP_DEGREES / RING_SLICES;
  const float c = cosf(degrees * kDegrees);
  const float s = sinf(degrees * kDegrees);
  const float inner = RING_RADIUS_PX - kReach;
  const float outer = RING_RADIUS_PX + kReach;
  return {(int)lroundf(kCentre + inner * c), (int)lroundf(kCentre + inner * s),
          (int)lroundf(kCentre + outer * c), (int)lroundf(kCentre + outer * s)};
}

Indicators::Ramp Indicators::rampFor(bool work, bool countingUp, int seconds) {
  if (countingUp) return seconds < FLOW_LAP_SECONDS ? Ramp::ToGreen : Ramp::ToCyan;
  return work ? Ramp::ToGreen : Ramp::ToRed;
}

uint32_t Indicators::rampColor(Ramp ramp, int remainingPercent, bool flow) {
  const uint32_t green = flow ? FLOW_ARC_COLOR_GREEN : ARC_COLOR_GREEN;
  const uint32_t amber = flow ? FLOW_ARC_COLOR_AMBER : ARC_COLOR_AMBER;
  const uint32_t red = flow ? FLOW_ARC_COLOR_RED : ARC_COLOR_RED;
  const uint32_t cyan = flow ? FLOW_ARC_COLOR_CYAN : ARC_COLOR_CYAN;

  switch (ramp) {
    case Ramp::ToGreen:
      return threeStop(remainingPercent, red, amber, green);
    case Ramp::ToRed:
      return threeStop(remainingPercent, green, amber, red);
    case Ramp::ToCyan:
      return twoStop(remainingPercent, green, cyan);
  }
  return green;
}

int Indicators::lapPercent(int elapsedSeconds) {
  if (elapsedSeconds <= 0) return 0;
  return ((elapsedSeconds % FLOW_LAP_SECONDS) * 100) / FLOW_LAP_SECONDS;
}

Indicators::Palette Indicators::palette(Ramp ramp, int rampAt, bool flow, bool dim) {
  const uint32_t ramped = rampColor(ramp, rampAt, flow);

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

Indicators::Palette Indicators::alertPalette(bool inverted, bool work) {
  // The black panel's stops rather than colours of its own: each is what the
  // interval's ramp has been shading towards for its last quarter, so the alarm
  // arrives as the end of that journey rather than as a new idea. Flow's darker
  // stops are left out -- the flash is a black field either way, and a break
  // finishing reads the same whichever face it was taken on.
  const uint32_t colour = work ? ARC_COLOR_GREEN : ARC_COLOR_RED;
  const uint32_t face = inverted ? colour : SCREEN_BG_COLOR;
  const uint32_t drawn = inverted ? SCREEN_BG_COLOR : colour;

  // Track set to the face so the ring leaves no groove behind even if something
  // fails to hide the arc itself. Battery takes the drawn colour for the same
  // reason it does when dim: a colour on itself is nothing at all.
  return {face, drawn, face, face, drawn};
}

Indicators::ClockFields Indicators::clockFields(int seconds) {
  if (seconds < 0) seconds = 0;
  if (seconds >= 3600) return {seconds / 3600, (seconds % 3600) / 60, true};
  return {seconds / 60, seconds % 60, false};
}

Indicators::ClockText Indicators::clockText(int seconds) {
  const ClockFields fields = clockFields(seconds);
  ClockText out;
  snprintf(out.text, sizeof out.text, fields.hours ? "%dh%02d" : "%02d:%02d", fields.left,
           fields.right);
  return out;
}

bool Indicators::showLowBattery(float batteryVoltage) {
  return batteryVoltage < LOW_BATTERY_VOLTAGE;
}

Indicators::FlipFrame Indicators::flipFrame(unsigned long ms) {
  constexpr unsigned long half = FLIP_GLYPH_HOLD_MS + FLIP_GLYPH_TURN_MS;
  const unsigned long t = ms % (2 * half);
  const bool startedOnBack = t >= half;
  const unsigned long into = t % half;

  // Eased in and out, as a hand turns it: slow to start, slow to land.
  float turned = 0;
  if (into > FLIP_GLYPH_HOLD_MS) {
    const float x = (float)(into - FLIP_GLYPH_HOLD_MS) / (float)FLIP_GLYPH_TURN_MS;
    turned = x * x * (3 - 2 * x);
  }

  // The width a card has seen from in front, turned this far through a half
  // turn; past the quarter it is the other side that faces out.
  const float angle = turned * (float)M_PI;
  const int width = (int)lroundf(fabsf(cosf(angle)) * 100);
  return {width, startedOnBack != (turned > 0.5f)};
}
