// The firmware includes <Wire.h> but, once the IMU driver is replaced by the
// simulator's stub, nothing on the host actually talks I2C.
#pragma once

#include <cstdint>

class TwoWire {
public:
  void begin() {}
  void begin(int sda, int scl) { (void)sda; (void)scl; }
};

extern TwoWire Wire;
