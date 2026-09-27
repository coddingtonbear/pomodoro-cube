#pragma once

namespace SimHost {

// Installed by the SDL layer so that blocking delay() calls inside the
// firmware (the deep-sleep path, mainly) still pump the window's event queue
// and repaint, instead of freezing the UI.
extern void (*delayHook)(unsigned long ms);

}  // namespace SimHost
