#pragma once
#include "consts.h"


namespace Util {

void updateBattery();
Orientation calcOrientation(float ax, float ay, float az);

// True for the two faces the cube rests flat on, neither of which runs a timer.
bool isRestingFace(Orientation ori);
void deepSleep(bool playSound);
int getTimerByOrientation(Orientation ori);
bool updateOriDebounce(Orientation rawState);
Orientation getDebouncedOriState();
}