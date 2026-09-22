// The simulated GC9A01: a 240x240 RGB565 framebuffer plus the little bit of
// panel state (rotation, backlight, sleep) the firmware drives through
// TFT_eSPI. The SDL layer reads this; nothing here knows about SDL.
#pragma once

#include <cstdint>

namespace SimPanel {

constexpr int WIDTH = 240;
constexpr int HEIGHT = 240;

extern uint16_t framebuffer[WIDTH * HEIGHT];

// Set whenever the firmware writes pixels, cleared by the renderer.
extern bool dirty;
extern bool backlightOn;
extern bool asleep;

void setRotation(uint8_t rotation);
uint8_t rotation();

// Mirrors TFT_eSPI's setAddrWindow/pushColors pair: open a rectangle in
// *rotated* coordinates, then stream pixels into it row by row.
void beginWindow(int x, int y, int w, int h);
void pushPixels(const uint16_t *colors, uint32_t count);

// Index into framebuffer[] of the physical pixel that the firmware addresses
// as (x, y) under the current rotation. Lets the renderer undo the rotation to
// show the panel the way someone holding the cube on that face would see it.
int panelIndex(int x, int y);

void sleepIn();
void wakeUp();
void clear(uint16_t colour);

}  // namespace SimPanel
