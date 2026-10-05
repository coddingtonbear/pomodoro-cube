#pragma once

// Colours shared between the generated UI setup and the C++ display code, so
// the two halves can't drift apart. Plain #defines rather than constexpr
// because the UI files are C, and consts.h (enum class, constexpr arrays) is
// not safe to include from them.

// The stops the arc's ramps run between. Which way it runs says what the end
// of the interval asks of you: work runs red to green, because green is the
// break it is heading for, and a break runs green to red, because red is the
// work waiting at the end of it. Amber is the midpoint either way round.
#define ARC_COLOR_GREEN 0x35C759
#define ARC_COLOR_AMBER 0xFFCC00
#define ARC_COLOR_RED 0xFF3B30
// Flow's later laps run green to cyan rather than repeating red to green: the
// break was earned on the first lap, and what each one after it adds is
// another pomodoro. Only flow draws it, but the black panel gets a stop too so
// that every ramp has both.
#define ARC_COLOR_CYAN 0x30D0F0

// The groove the arc runs in. Nearly invisible against the dark panel, which is
// the intent; flow mode's white one needs its own or the ring reads as a solid
// dark band.
#define ARC_TRACK_COLOR 0x202020

// A paused timer is frozen on screen while the CPU sleeps, so it has to look
// unmistakably different from a running one at a glance.
#define ARC_COLOR_PAUSED 0x5A6472
#define SCREEN_BG_COLOR 0x000000
#define COUNTDOWN_COLOR 0xFFFFFF
#define COUNTDOWN_COLOR_PAUSED 0x6E7A86

// Flow mode inverts the panel, so a stint counting up can't be mistaken for a
// countdown from across the room.
#define FLOW_BG_COLOR 0xFFFFFF
#define COUNTDOWN_COLOR_FLOW 0x000000

// The same stops as the countdown arc, a step darker at every one: those
// colours were picked against black, and amber -- or cyan -- on white is all
// but invisible. The cyan is a deep teal for the same reason.
#define FLOW_ARC_TRACK_COLOR 0xDCDCDC
#define FLOW_ARC_COLOR_GREEN 0x1E7B34
#define FLOW_ARC_COLOR_AMBER 0x8A6D00
#define FLOW_ARC_COLOR_RED 0xC1271D
#define FLOW_ARC_COLOR_CYAN 0x00809E

#define LOW_BATTERY_COLOR 0xFF3B30
