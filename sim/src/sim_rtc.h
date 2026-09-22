#pragma once

// Stands in for the ESP32's RTC memory, which keeps its contents through deep
// sleep and loses them on a power cycle. The simulator's "deep sleep" re-execs
// the process, so the block is persisted to a file beside the binary and
// reloaded on the way back up.
namespace SimRtc {

// Call before setup(). `wokeFromSleep` distinguishes a wake from deep sleep
// (block restored) from a cold power-on (block filled with junk, as real RTC
// memory would be).
void restore(bool wokeFromSleep);

// Call when the firmware enters deep sleep, before the process re-execs.
void persist();

}  // namespace SimRtc
