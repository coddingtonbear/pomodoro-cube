#pragma once

namespace SimHost {

// Installed by the SDL layer so that blocking delay() calls inside the
// firmware (the beeper sequences, mainly) still pump the window's event queue
// and repaint, instead of freezing the UI.
extern void (*delayHook)(unsigned long ms);

}  // namespace SimHost
