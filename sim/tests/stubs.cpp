// util.cpp reaches into the display, battery, beeper, IMU and BLE on its
// deep-sleep path. None of that hardware exists here; the stubs record the
// calls instead, so the order the path makes them in can be tested.
#include "stubs.h"

#include "battery.h"
#include "ble.h"
#include "beeper.h"
#include "display.h"
#include "qmi.h"

std::vector<std::string> Stubs::calls;

void Stubs::clear() { calls.clear(); }

namespace {
void called(const char *what) { Stubs::calls.push_back(what); }
}  // namespace

float Battery::getVoltage() { return 3.9f; }
void Battery::sleepIfEmpty() {}
void Battery::cycleBatteryUpdate() {}

void Display::setup() {}
void Display::updateBattery(float voltage) { (void)voltage; }
void Display::deepSleep() { called("Display::deepSleep"); }
void Display::holdPausedFrame() { called("Display::holdPausedFrame"); }
void Display::showPaused() {}
void Display::rotateScreen(Orientation ori) { (void)ori; }
void Display::updateTimer(const TimerView &view) { (void)view; }
void Display::cycleTimerFinish() {}

void Beeper::setup() {}
void Beeper::beep(unsigned int frequency, unsigned int duration) { (void)frequency; (void)duration; }
bool Beeper::cycleBeeper() { return false; }
void Beeper::playWakeUp() {}
void Beeper::playShutdown() { called("Beeper::playShutdown"); }

void QMI::setup() {}
void QMI::setupWakeup() { called("QMI::setupWakeup"); }
void QMI::enableTapDetection() {}
bool QMI::takeTap() { return false; }
bool QMI::getAccelerometer(float &ax, float &ay, float &az) {
  ax = 0.0f;
  ay = 0.0f;
  az = 0.0f;
  return false;
}

void BLE::setup() {}
void BLE::publish(const BTHome::State &state) { (void)state; }
void BLE::farewell() { called("BLE::farewell"); }
void BLE::shutdown() { called("BLE::shutdown"); }
