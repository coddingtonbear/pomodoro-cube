#pragma once

// What the stubbed-out peripherals in stubs.cpp saw, for tests of the order in
// which the deep-sleep path talks to them. Every entry is a call the real
// firmware would make to a device; the shim's esp_deep_sleep_start() is the
// one thing here that is not a stub, being recorded by the shim itself.
#include <string>
#include <vector>

namespace Stubs {

// The calls that have been made since the last clear(), in order.
extern std::vector<std::string> calls;

void clear();

}  // namespace Stubs
