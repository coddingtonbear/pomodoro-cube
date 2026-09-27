#include "haptic_pattern.h"

#include "consts.h"

namespace {

// How long each step lasts, alternating motor on and motor off and starting
// with on.
struct Steps {
  const int *durationsMs;
  int count;
  bool repeats;
};

template <int N>
constexpr Steps stepsOf(const int (&durations)[N], bool repeats) {
  return {durations, N, repeats};
}

Steps stepsFor(Haptic::Pattern pattern) {
  switch (pattern) {
    case Haptic::Pattern::Wake: return stepsOf(HAPTIC_WAKE_MS, false);
    case Haptic::Pattern::FaceChange: return stepsOf(HAPTIC_FACE_CHANGE_MS, false);
    case Haptic::Pattern::Alarm: return stepsOf(HAPTIC_ALARM_MS, true);
    case Haptic::Pattern::None: break;
  }
  return {nullptr, 0, false};
}

unsigned long totalMs(const Steps &steps) {
  unsigned long total = 0;
  for (int i = 0; i < steps.count; i++) total += (unsigned long)steps.durationsMs[i];
  return total;
}

}  // namespace

bool Haptic::isFinished(Pattern pattern, unsigned long elapsedMs) {
  const Steps steps = stepsFor(pattern);
  if (steps.repeats) return false;
  return elapsedMs >= totalMs(steps);
}

bool Haptic::motorOn(Pattern pattern, unsigned long elapsedMs) {
  const Steps steps = stepsFor(pattern);
  const unsigned long total = totalMs(steps);
  if (total == 0) return false;

  if (steps.repeats) elapsedMs %= total;
  else if (elapsedMs >= total) return false;

  unsigned long stepEnds = 0;
  for (int i = 0; i < steps.count; i++) {
    stepEnds += (unsigned long)steps.durationsMs[i];
    if (elapsedMs < stepEnds) return i % 2 == 0;
  }
  return false;
}

void Haptic::Player::play(Pattern pattern, unsigned long nowMs) {
  if (pattern == pattern_ && stepsFor(pattern).repeats) return;
  pattern_ = pattern;
  startedMs_ = nowMs;
}

void Haptic::Player::stop() {
  pattern_ = Pattern::None;
}

bool Haptic::Player::update(unsigned long nowMs) {
  const unsigned long elapsed = nowMs - startedMs_;
  if (isFinished(pattern_, elapsed)) pattern_ = Pattern::None;

  const bool on = motorOn(pattern_, elapsed);
  if (on) {
    hasRun_ = true;
    lastOnMs_ = nowMs;
  }
  return on;
}

Haptic::Pattern Haptic::Player::playing() const {
  return pattern_;
}

bool Haptic::Player::disturbing(unsigned long nowMs) const {
  return hasRun_ && nowMs - lastOnMs_ < (unsigned long)HAPTIC_SETTLE_MS;
}
