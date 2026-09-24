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

// What standing the cube on a face asks for. `seconds` is the length to count
// down; it is 0 in CountUp, where there is no length to count. `spendsBank` is
// true when that countdown *is* the flow bank draining, so the caller knows to
// keep the bank in step as it goes.
struct TimerSpec {
  TimerKind kind;
  TimerMode mode;
  int seconds;
  bool spendsBank;
};

// The whole face table in one place. `bankedBreakSeconds` is the flow bank,
// which only the flow break face reads. Every other face ignores it -- and
// leaves it alone: a bank is not forfeited by working somewhere else.
TimerSpec getTimerSpec(Orientation ori, int bankedBreakSeconds);

// What a flow stint of this length adds to the bank: a fifth of it.
int flowBreakCredit(int workedSeconds);

// How long the flow break face counts down for, given the bank. The bank if
// there is one, however small -- an account pays out what it holds -- and the
// fixed fallback only when the face was chosen with nothing banked at all.
int flowBreakSeconds(int bankedSeconds);

// True on the second a flow lap completes -- the moment the arc comes back
// round -- which is when a stint scores a pomodoro. Counting by the lap rather
// than by the stint is what makes a long stint worth what it actually was: two
// hours of flow is four pomodoros, not one.
bool completesFlowLap(int elapsedSeconds);

bool updateOriDebounce(Orientation rawState);
Orientation getDebouncedOriState();
}
