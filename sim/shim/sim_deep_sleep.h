#pragma once

// Thrown by the shim's esp_deep_sleep_start(). The simulator's main loop
// catches it, blanks the panel and waits for a wake keypress, which mirrors
// how the real cube parks itself until the IMU interrupt fires.
struct SimDeepSleep {};
