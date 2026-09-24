#pragma once
#include "consts.h"
#include "rtc_state.h"


namespace Util {

void updateBattery();

// The smoothed pack voltage -- the same figure the panel shows, rather than a
// fresh reading, so the screen and the advertisement never disagree about what
// the battery is doing. Zero until updateBattery() has run at least once.
float batteryVolts();

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

// What the panel is worth lighting for. Deliberately not a snapshot of the
// whole timer: brightness answers "is there something to look at just now",
// which is a narrower question than what the face is showing.
struct BacklightView {
  // Since the cube was last set on a different face. That change is a decision
  // the user just made, and the figure that came up is what they made it for.
  unsigned long sinceFaceChangeMs;
  // Remaining on a countdown, including 0 for one that has finished and is
  // beeping -- the state that most needs to be seen from across a room.
  int remainingSeconds;
  // A stint counting up has no end to approach, so it never brightens on its
  // own; only turning the cube off it is a moment.
  bool countingUp;
  // Since the cube was last tapped, which is someone asking to read it. Must be
  // past the window when there has been no tap at all: zero reads as "just
  // tapped" and would light the panel for the first ten seconds of every boot.
  unsigned long sinceTapMs;
};

// How bright the backlight should be, as a percentage.
int backlightPercent(const BacklightView &view);

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

// Feed a raw orientation reading in and get back whether the debounced state
// just changed. A reading has to hold still for ORI_DEBOUNCE_DELAY before it is
// accepted: anything that flickers restarts the clock, so a cube in mid-air
// settles on the face it is finally put down on rather than on whichever sample
// happened to land at the end of the window. `nowMs` is millis() on the device
// and a supplied clock in the tests.
bool updateOriDebounce(Orientation rawState, unsigned long nowMs);
Orientation getDebouncedOriState();

// Forget everything the debouncer has seen. For the tests; the firmware gets a
// fresh one from every cold boot.
void resetOriDebounce();
}
