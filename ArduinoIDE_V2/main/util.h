#pragma once
#include "consts.h"
#include "rtc_state.h"


namespace Util {

void updateBattery();

// The smoothed pack voltage -- the same figure the panel shows, rather than a
// fresh reading, so the screen and the advertisement never disagree about what
// the battery is doing. Zero until updateBattery() has run at least once.
float batteryVolts();

// Which way is down, by whichever axis carries the most of gravity. Every
// reading names a face: there is no threshold to fall short of and no
// fall-through to a face the cube is not on.
Orientation calcOrientation(float ax, float ay, float az);

// True when a reading is gravity and very little else, which is the only kind
// that says anything about which way up the cube is. Screens out the cube being
// carried or knocked, and the handful of nonsense samples the QMI8658 emits
// while it comes up after being configured.
bool isGravityOnly(float ax, float ay, float az);

// True when the dominant axis leads the runner-up by enough to name a face
// outright, rather than the cube sitting between two of them.
bool isDecisive(float ax, float ay, float az);

// True for the two faces the cube rests flat on, neither of which runs a timer.
bool isRestingFace(Orientation ori);
// Off blanks the panel; Paused leaves the frozen frame lit.
enum class SleepMode { Off, Paused };
// `announce` buzzes the motor once on the way down, for a sleep the user asked
// for by setting the cube down on a resting face.
void deepSleep(SleepMode mode, bool announce);

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
// Face down also switches the cube off -- see RtcState::Data::switchedOff.
RestPlan restOnFace(RtcState::Data &data, Orientation ori, Orientation timerFace,
                    int remaining, int selected, bool countingUp);

// Park a work timer that has not started, if nothing is parked already: what a
// cube switched on with nothing to resume shows, paused, until it is stood on a
// face. Standing it on the work face takes it up, which is the timer that face
// would have started anyway, and any other face abandons it at no cost.
void parkFreshTimer(RtcState::Data &data);

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
  // buzzing -- the state that most needs to be seen from across a room.
  int remainingSeconds;
  // A stint counting up has no end to approach. What it has instead is laps,
  // and it brightens either side of each one closing; otherwise only turning
  // the cube off it is a moment.
  bool countingUp;
  // Since the cube was last tapped, which is someone asking to read it. Must be
  // past the window when there has been no tap at all: zero reads as "just
  // tapped" and would light the panel for the first ten seconds of every boot.
  unsigned long sinceTapMs;
  // Parked face up with the cube still awake. The figures are standing still,
  // so how few seconds are left is no reason to light them. Last, and without
  // a default: the device builds as C++11, where a default would stop this
  // being an aggregate, and left off a braced list it is false anyway.
  bool paused;
  // Into the current second, 0-999. Counting up, `remainingSeconds` is the
  // elapsed time, and this is what places a lap's closing finer than a whole
  // second. Left off, it is 0: the start of the second.
  unsigned long sinceTickMs;
};

// How bright the backlight should be, as a percentage.
int backlightPercent(const BacklightView &view);

// The CPU clock to run at while the backlight is at this percentage.
uint32_t cpuMhz(int backlightPercent);

struct PaceView {
  int backlightPercent;
  // The motor is running a pattern, or a finished timer is flashing.
  bool busy;
  unsigned long sinceMovedMs;
  // Until the next second is due to tick, or past any pass when nothing is
  // counting.
  unsigned long untilTickMs;
};

// Whether nothing on the cube needs the loop fast.
bool loopIsIdle(const PaceView &view);

// How long from the start of this pass to the start of the next.
uint32_t loopPassMs(const PaceView &view);

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

// Listens, on a wake of a switched-off cube, for the gesture that switches it
// back on: flipped SWITCH_ON_FLIPS times, each flip ending face up, with no
// more than SWITCH_ON_FLIP_WINDOW_MS for each. Fed a pass at a time, and says
// when it has heard enough either way. Also drives the orientation debouncer
// below, which a wake starts fresh, for what it says about a cube left alone.
enum class SwitchOnVerdict { Listening, On, StayOff };

class SwitchOnListener {
 public:
  explicit SwitchOnListener(unsigned long nowMs);
  SwitchOnVerdict update(bool haveReading, float ax, float ay, float az, unsigned long nowMs);

  // Flips still to make: SWITCH_ON_FLIPS until the first one lands.
  int flipsToGo() const { return SWITCH_ON_FLIPS - flips_; }

 private:
  enum class Side { Up, Down, Neither };
  static Side sideOf(float ax, float ay, float az);

  // When the listening ends: the wake, then each flip, plus the window.
  unsigned long deadline_;
  // The last reading that was not the cube resting on its current face.
  unsigned long lastDisturbed_;
  int flips_ = 0;
  // The side the cube was last held on for FLIP_HOLD_MS. Starts face down,
  // which is where every switched-off cube was put to sleep -- and so a turn
  // face up that is already over by the first reading still counts.
  Side side_ = Side::Down;
  // What the readings currently say, and since when.
  Side candidate_ = Side::Neither;
  unsigned long candidateSince_ = 0;
};

// Feed a raw accelerometer reading in and get back whether the debounced face
// just changed. Takes the vector rather than an orientation because the two
// reasons to distrust a reading -- it is not gravity, or it does not clearly
// name a face -- are both properties of the vector, and both have to restart
// the clock rather than be classified.
//
// A reading has to hold still for ORI_DEBOUNCE_DELAY before it is accepted:
// anything that flickers restarts the clock, so a cube in mid-air settles on
// the face it is finally put down on rather than on whichever sample happened
// to land at the end of the window. `nowMs` is millis() on the device and a
// supplied clock in the tests.
bool updateOriDebounce(float ax, float ay, float az, unsigned long nowMs);
Orientation getDebouncedOriState();

// Forget everything the debouncer has seen. For the tests; the firmware gets a
// fresh one from every cold boot.
void resetOriDebounce();
}
