#pragma once
#include "consts.h"


namespace Util {

void updateBattery();
Orientation calcOrientation(float ax, float ay, float az);

// True for the two faces the cube rests flat on, neither of which runs a timer.
bool isRestingFace(Orientation ori);
// Off blanks the panel; Paused leaves the frozen frame lit.
enum class SleepMode { Off, Paused };
void deepSleep(SleepMode mode, bool playSound);
int getTimerByOrientation(Orientation ori);
bool updateOriDebounce(Orientation rawState);
Orientation getDebouncedOriState();
}