#pragma once
#include "consts.h"

namespace Display {

void setup();
void updateBattery(float voltage);
void deepSleep();
void rotateScreen(Orientation ori);
void updateTimer(int seconds, int selSeconds);
void cycleTimerFinish();
}