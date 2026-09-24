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
// down; it is 0 in CountUp, where there is no length to count.
struct TimerSpec {
  TimerKind kind;
  TimerMode mode;
  int seconds;
};

// The whole face table in one place. `earnedFlowSeconds` is the flow stint
// waiting to be spent, which only the flow break face reads -- every other face
// ignores it, so turning to one of them forfeits the break, the same way it
// abandons a pause.
TimerSpec getTimerSpec(Orientation ori, int earnedFlowSeconds);

// How long a break a flow stint of this length has earned: a fifth of it, or
// the fallback break when the stint was too short to earn anything.
int flowBreakSeconds(int earnedSeconds);

// True on the second a flow lap completes -- the moment the arc comes back
// round -- which is when a stint scores a pomodoro. Counting by the lap rather
// than by the stint is what makes a long stint worth what it actually was: two
// hours of flow is four pomodoros, not one.
bool completesFlowLap(int elapsedSeconds);

bool updateOriDebounce(Orientation rawState);
Orientation getDebouncedOriState();
}
