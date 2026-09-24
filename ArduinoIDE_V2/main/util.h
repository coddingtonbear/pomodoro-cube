#pragma once
#include "consts.h"
#include "rtc_state.h"


namespace Util {

void updateBattery();
Orientation calcOrientation(float ax, float ay, float az);

// True for the two faces the cube rests flat on, neither of which runs a timer.
bool isRestingFace(Orientation ori);
// Off blanks the panel; Paused leaves the frozen frame lit.
enum class SleepMode { Off, Paused };
void deepSleep(SleepMode mode, bool playSound);

// What setting the cube down on a resting face asks for. `lit` is the whole
// difference between the two faces: face up shows the paused figures, face down
// goes dark. Both sleep the same way otherwise.
struct RestPlan {
  bool lit;
  SleepMode mode;
};

// Park what was running because the cube was set down. Setting it down is not
// abandoning the interval, so both faces keep the pause, and neither touches the
// break bank: a balance outlives being put away. `timerFace` is the face the
// timer was running on, which is the only face the pause will resume onto.
RestPlan restOnFace(RtcState::Data &data, Orientation ori, Orientation timerFace,
                    int remaining, int selected, bool countingUp);

// What standing the cube on a face asks for. `seconds` is the length to count
// down; it is 0 in CountUp, where there is no length to count, and on the flow
// break face with an empty bank, where there is no break to take. `spendsBank`
// is true when that countdown *is* the flow bank draining, so the caller knows
// to keep the bank in step as it goes.
struct TimerSpec {
  TimerKind kind;
  TimerMode mode;
  int seconds;
  // One of flow's two faces. Both invert the panel: what makes flow worth
  // marking out is the bank behind it, which the break face is spending just as
  // much as the work face is filling it.
  bool flow;
  // This countdown is the bank draining, so the caller keeps the bank in step
  // as it goes. Implies `flow` and a Break kind; the fixed faces never set it.
  bool spendsBank;
};

// The whole face table in one place. `bankedBreakSeconds` is the flow bank,
// which only the flow break face reads. Every other face ignores it -- and
// leaves it alone: a bank is not forfeited by working somewhere else.
TimerSpec getTimerSpec(Orientation ori, int bankedBreakSeconds);

// What a flow stint of this length adds to the bank: a fifth of it.
int flowBreakCredit(int workedSeconds);

// What the bank would be worth if a stint of `elapsedSeconds` ended now, which
// is the figure the flow work face shows: the question you are asking when you
// look at it is what you would get by turning the cube over.
int flowBankPreview(int bankedSeconds, int elapsedSeconds);

// True on the second a flow lap completes -- the moment the arc comes back
// round -- which is when a stint scores a pomodoro. Counting by the lap rather
// than by the stint is what makes a long stint worth what it actually was: two
// hours of flow is four pomodoros, not one.
bool completesFlowLap(int elapsedSeconds);

bool updateOriDebounce(Orientation rawState);
Orientation getDebouncedOriState();
}
