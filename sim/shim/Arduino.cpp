#include <Arduino.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "Wire.h"
#include "consts.h"
#include "sim_deep_sleep.h"
#include "sim_host.h"
#include "sim_input.h"
#include "sim_panel.h"

TwoWire Wire;
SimSerial Serial;

namespace SimHost {
void (*delayHook)(unsigned long ms) = nullptr;
}

namespace {
const std::chrono::steady_clock::time_point kBoot = std::chrono::steady_clock::now();

// SIM_TIME_SCALE, read once: how many times faster than the wall clock the
// firmware's time runs. Anything missing, unparseable or not positive is 1.
double readTimeScale() {
  const char *value = std::getenv("SIM_TIME_SCALE");
  if (!value) return 1.0;
  const double scale = std::atof(value);
  return scale > 0.0 ? scale : 1.0;
}

// Wall-clock time since boot, sped up by the time scale.
std::chrono::duration<double, std::micro> scaledSinceBoot() {
  using namespace std::chrono;
  return duration<double, std::micro>(steady_clock::now() - kBoot) * SimHost::timeScale();
}
}  // namespace

double SimHost::timeScale() {
  static const double scale = readTimeScale();
  return scale;
}

unsigned long SimHost::wallMs(unsigned long simMs) {
  return (unsigned long)((double)simMs / timeScale());
}

unsigned long millis() {
  return (unsigned long)(scaledSinceBoot().count() / 1000.0);
}

unsigned long micros() {
  return (unsigned long)scaledSinceBoot().count();
}

void delay(unsigned long ms) {
  if (SimHost::delayHook) {
    SimHost::delayHook(ms);
    return;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(SimHost::wallMs(ms)));
}

void delayMicroseconds(unsigned int us) {
  std::this_thread::sleep_for(std::chrono::microseconds(us) / SimHost::timeScale());
}

void pinMode(uint8_t pin, uint8_t mode) { (void)pin; (void)mode; }

void digitalWrite(uint8_t pin, uint8_t value) {
  // The backlight pin is the one GPIO whose state the simulated panel cares
  // about. Driven directly it is the sleep paths parking it, which are always
  // full on or fully off.
  if (pin == TFT_BL_PIN) SimPanel::backlightPercent = (value == HIGH) ? 100 : 0;
  if (pin == HAPTIC_PIN) SimInput::motorActive = (value == HIGH);
}

bool ledcAttach(uint8_t pin, uint32_t frequency, uint8_t resolution) {
  (void)pin;
  (void)frequency;
  (void)resolution;
  return true;
}

bool ledcWrite(uint8_t pin, uint32_t duty) {
  // The firmware asks for a percentage and converts to 8-bit duty; undo that so
  // the renderer can dim by the figure the policy actually chose.
  if (pin == TFT_BL_PIN) SimPanel::backlightPercent = (int)((duty * 100 + 127) / 255);
  return true;
}

bool ledcDetach(uint8_t pin) {
  (void)pin;
  return true;
}
int digitalRead(uint8_t pin) { (void)pin; return LOW; }
int analogRead(uint8_t pin) { (void)pin; return 0; }

uint32_t analogReadMilliVolts(uint8_t pin) {
  (void)pin;
  // Undo the Waveshare 200k/100k divider that Battery::getVoltage() re-applies,
  // so the firmware reads back whatever pack voltage the sim is pretending to have.
  return (uint32_t)((SimInput::batteryVoltage / 3.0f) * 1000.0f);
}

static uint32_t g_cpuMhz = 240;
bool setCpuFrequencyMhz(uint32_t mhz) { g_cpuMhz = mhz; return true; }
uint32_t getCpuFrequencyMhz() { return g_cpuMhz; }
void gpio_hold_en(gpio_num_t pin) { (void)pin; }
void gpio_hold_dis(gpio_num_t pin) { (void)pin; }
void gpio_deep_sleep_hold_en() {}
void esp_sleep_enable_ext0_wakeup(gpio_num_t pin, int level) { (void)pin; (void)level; }

void esp_deep_sleep_start() { throw SimDeepSleep{}; }

void enableLoopWDT() {}
void feedLoopWDT() {}
esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }

void SimSerial::begin(unsigned long baud) { (void)baud; }
void SimSerial::print(const char *s) { std::fputs(s, stdout); }
void SimSerial::println(const char *s) { std::printf("%s\n", s); }

void SimSerial::printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::vprintf(fmt, args);
  va_end(args);
}
