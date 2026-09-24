// util.cpp reaches into the display, battery, beeper and IMU on its deep-sleep
// path. The tests don't exercise that path, but the linker still needs the
// symbols.
#include "battery.h"
#include "beeper.h"
#include "display.h"
#include "qmi.h"

float Battery::getVoltage() { return 3.9f; }
void Battery::sleepIfEmpty() {}
void Battery::cycleBatteryUpdate() {}

void Display::setup() {}
void Display::updateBattery(float voltage) { (void)voltage; }
void Display::deepSleep() {}
void Display::holdPausedFrame() {}
void Display::showPaused() {}
void Display::rotateScreen(Orientation ori) { (void)ori; }
void Display::updateTimer(const TimerView &view) { (void)view; }
void Display::cycleTimerFinish() {}

void Beeper::setup() {}
void Beeper::beep(unsigned int frequency, unsigned int duration) { (void)frequency; (void)duration; }
bool Beeper::cycleBeeper() { return false; }
void Beeper::playWakeUp() {}
void Beeper::playShutdown() {}

void QMI::setup() {}
void QMI::setupWakeup() {}
bool QMI::getAccelerometer(float &ax, float &ay, float &az) {
  ax = 0.0f;
  ay = 0.0f;
  az = 0.0f;
  return false;
}
