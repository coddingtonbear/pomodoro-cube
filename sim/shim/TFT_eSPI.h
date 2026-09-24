// Just enough of Bodmer's TFT_eSPI to satisfy display.cpp. Pixel writes land
// in the simulated panel framebuffer instead of on an SPI bus.
#pragma once

#include <cstdint>

// display.cpp refuses to build against a TFT_eSPI that is not configured for
// this panel. The shim is that configuration, by standing in for the whole
// library, so say so.
#define GC9A01_DRIVER

// Mirrors ArduinoIDE_V2/User_Setup.h: display.cpp holds the panel's reset line
// through a paused sleep and so needs the pin number the real library's
// configuration would have handed it.
#define TFT_RST 14

class TFT_eSPI {
public:
  void begin();
  void setRotation(uint8_t rotation);
  void startWrite();
  void endWrite();
  void setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h);
  void pushColors(uint16_t *data, uint32_t len, bool swapBytes = true);
  void writecommand(uint8_t command);
};
