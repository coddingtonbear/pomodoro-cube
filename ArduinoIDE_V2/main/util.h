#pragma once
#include "consts.h"


namespace Util {

void updateBattery();
Orientation calcOrientation(float ax, float ay, float az);
void deepSleep(bool playSound);
int getTimerByOrientation(Orientation ori);
bool updateOriDebounce(Orientation rawState);
Orientation getDebouncedOriState();
}