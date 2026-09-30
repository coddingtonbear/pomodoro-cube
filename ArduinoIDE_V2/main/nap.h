#pragma once
#include <stdint.h>

// Light sleep between passes of the loop. The CPU stops and the chip drops to a
// few milliamps, with RAM, the pins and the panel's frame all kept, and comes
// back where it left off -- unlike deep sleep, which is a reboot. What has to be
// arranged is that everything the cube shows carries on without the CPU: the
// panel holds its own frame, but the backlight's PWM needs a clock that light
// sleep leaves running.
//
// Hardware only: the simulator's stand-in in sim/src/sim_nap.cpp just waits.
namespace Nap {

// Moves the LEDC timer behind `channel` onto the internal 8 MHz oscillator and
// keeps that oscillator powered through light sleep. The core's default clock
// is the 40 MHz crystal, which light sleep turns off: the PWM would stop
// wherever it was, and a dimmed panel would sit at full or go dark for every
// nap. Call after the channel has been set up by ledcSetup().
void keepPwmThroughSleep(int channel, uint32_t frequency, uint8_t bits);

// Waits `ms`, asleep rather than spinning. Returns once the time is up.
void sleepFor(uint32_t ms);

}  // namespace Nap
