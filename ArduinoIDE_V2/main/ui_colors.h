#pragma once

// Colours shared between the generated UI setup and the C++ display code, so
// the two halves can't drift apart. Plain #defines rather than constexpr
// because the UI files are C, and consts.h (enum class, constexpr arrays) is
// not safe to include from them.

// Countdown arc, from a full timer down to an expired one.
#define ARC_COLOR_FULL 0x35C759
#define ARC_COLOR_MID 0xFFCC00
#define ARC_COLOR_LOW 0xFF3B30

// The groove the arc runs in. Nearly invisible against the dark panel, which is
// the intent; flow mode's white one needs its own or the ring reads as a solid
// dark band.
#define ARC_TRACK_COLOR 0x202020

// The dark half of the pulse once the timer has finished.
#define ARC_COLOR_FINISH_DIM 0x401512

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

// The same green-to-red journey as the countdown arc, a step darker at every
// stop: those colours were picked against black, and amber on white is all but
// invisible.
#define FLOW_ARC_TRACK_COLOR 0xDCDCDC
#define FLOW_ARC_COLOR_FULL 0x1E7B34
#define FLOW_ARC_COLOR_MID 0x8A6D00
#define FLOW_ARC_COLOR_LOW 0xC1271D

#define LOW_BATTERY_COLOR 0xFF3B30
