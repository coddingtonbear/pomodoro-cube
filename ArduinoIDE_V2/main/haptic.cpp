#include "haptic.h"

#include <Arduino.h>

#include "consts.h"

namespace {

Haptic::Player player;

void drive(bool on) {
  digitalWrite(HAPTIC_PIN, on ? HIGH : LOW);
}

}  // namespace

void Haptic::setup() {
  gpio_hold_dis((gpio_num_t)HAPTIC_PIN);
  pinMode(HAPTIC_PIN, OUTPUT);
  drive(false);
}

void Haptic::play(Pattern pattern) {
  player.play(pattern, millis());
  drive(player.update(millis()));
}

void Haptic::playBlocking(Pattern pattern) {
  if (!isFinished(pattern, ~0UL)) return;
  play(pattern);
  while (player.playing() != Pattern::None) {
    delay(5);
    cycle();
  }
}

void Haptic::stop() {
  player.stop();
  drive(false);
}

void Haptic::cycle() {
  drive(player.update(millis()));
}

Haptic::Pattern Haptic::playing() {
  return player.playing();
}

bool Haptic::disturbing() {
  return player.disturbing(millis());
}

void Haptic::holdForSleep() {
  stop();
  gpio_hold_en((gpio_num_t)HAPTIC_PIN);
  gpio_deep_sleep_hold_en();
}
