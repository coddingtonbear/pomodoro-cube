#pragma once

// Colours shared between the generated UI setup and the C++ display code, so
// the two halves can't drift apart. Plain #defines rather than constexpr
// because the UI files are C, and consts.h (enum class, constexpr arrays) is
// not safe to include from them.

// Countdown arc, from a full timer down to an expired one.
#define ARC_COLOR_FULL 0x35C759
#define ARC_COLOR_MID 0xFFCC00
#define ARC_COLOR_LOW 0xFF3B30

// The dark half of the pulse once the timer has finished.
#define ARC_COLOR_FINISH_DIM 0x401512

// A paused timer is frozen on screen while the CPU sleeps, so it has to look
// unmistakably different from a running one at a glance.
#define ARC_COLOR_PAUSED 0x5A6472
#define COUNTDOWN_COLOR 0xFFFFFF
#define COUNTDOWN_COLOR_PAUSED 0x6E7A86

#define LOW_BATTERY_COLOR 0xFF3B30
