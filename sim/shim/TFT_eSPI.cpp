#include "TFT_eSPI.h"

#include <cstdio>

#include "sim_panel.h"

namespace {
// GC9A01 command opcodes the firmware issues directly.
constexpr uint8_t CMD_SLPIN = 0x10;
constexpr uint8_t CMD_SLPOUT = 0x11;

uint16_t swap16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
}  // namespace

void TFT_eSPI::begin() {
  SimPanel::clear(0x0000);
  SimPanel::wakeUp();
}

void TFT_eSPI::setRotation(uint8_t rotation) { SimPanel::setRotation(rotation); }

void TFT_eSPI::startWrite() {}
void TFT_eSPI::endWrite() {}

void TFT_eSPI::setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h) {
  SimPanel::beginWindow((int)x, (int)y, (int)w, (int)h);
}

void TFT_eSPI::pushColors(uint16_t *data, uint32_t len, bool swapBytes) {
  if (!swapBytes) {
    SimPanel::pushPixels(data, len);
    return;
  }
  // Rare path here, so a small stack buffer walked in chunks is fine.
  uint16_t chunk[256];
  uint32_t done = 0;
  while (done < len) {
    uint32_t n = len - done;
    if (n > 256) n = 256;
    for (uint32_t i = 0; i < n; i++) chunk[i] = swap16(data[done + i]);
    SimPanel::pushPixels(chunk, n);
    done += n;
  }
}

void TFT_eSPI::writecommand(uint8_t command) {
  switch (command) {
    case CMD_SLPIN:  SimPanel::sleepIn(); break;
    case CMD_SLPOUT: SimPanel::wakeUp();  break;
    default:
      std::printf("[tft] unhandled command 0x%02X\n", command);
      break;
  }
}
