#include "sim_panel.h"

namespace SimPanel {

uint16_t framebuffer[WIDTH * HEIGHT] = {0};
bool dirty = true;
int backlightPercent = 0;
bool asleep = false;

namespace {
uint8_t g_rotation = 0;
int g_winX = 0, g_winY = 0, g_winW = 0, g_winH = 0;
int g_cursorX = 0, g_cursorY = 0;

// Map a coordinate in the firmware's rotated frame onto the physical panel,
// the same way the GC9A01's MADCTL register does on hardware.
void toPanel(int x, int y, int &px, int &py) {
  switch (g_rotation & 0x03) {
    case 1:  px = HEIGHT - 1 - y; py = x;               break;
    case 2:  px = WIDTH - 1 - x;  py = HEIGHT - 1 - y;  break;
    case 3:  px = y;              py = WIDTH - 1 - x;   break;
    default: px = x;              py = y;               break;
  }
}
}  // namespace

void setRotation(uint8_t rotation) {
  g_rotation = rotation & 0x03;
  dirty = true;
}

uint8_t rotation() { return g_rotation; }

void beginWindow(int x, int y, int w, int h) {
  g_winX = x;
  g_winY = y;
  g_winW = w;
  g_winH = h;
  g_cursorX = 0;
  g_cursorY = 0;
}

void pushPixels(const uint16_t *colors, uint32_t count) {
  if (g_winW <= 0 || g_winH <= 0) return;

  for (uint32_t i = 0; i < count; i++) {
    if (g_cursorY >= g_winH) break;  // window overrun; drop the extra pixels

    int px, py;
    toPanel(g_winX + g_cursorX, g_winY + g_cursorY, px, py);
    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
      framebuffer[py * WIDTH + px] = colors[i];
    }

    if (++g_cursorX >= g_winW) {
      g_cursorX = 0;
      g_cursorY++;
    }
  }
  dirty = true;
}

int panelIndex(int x, int y) {
  int px, py;
  toPanel(x, y, px, py);
  if (px < 0 || px >= WIDTH || py < 0 || py >= HEIGHT) return 0;
  return py * WIDTH + px;
}

void sleepIn() {
  asleep = true;
  dirty = true;
}

void wakeUp() {
  asleep = false;
  dirty = true;
}

void clear(uint16_t colour) {
  for (int i = 0; i < WIDTH * HEIGHT; i++) framebuffer[i] = colour;
  dirty = true;
}

}  // namespace SimPanel
