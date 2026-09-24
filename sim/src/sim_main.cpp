// Desktop entry point. Opens an SDL window standing in for the cube's round
// GC9A01, then drives the real firmware's setup()/loop() against the shims.
#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

#include "bthome.h"
#include "consts.h"
#include "display.h"
#include "util.h"
#include "sim_deep_sleep.h"
#include "sim_host.h"
#include "sim_input.h"
#include "sim_panel.h"
#include "sim_rtc.h"
#include "rtc_state.h"

// Defined by the firmware's main.ino.
void setup();
void loop();
extern int remSeconds;
extern int selSeconds;
extern TimerKind timerKind;
extern TimerMode timerMode;

namespace {

constexpr int kScale = 3;
constexpr uint16_t kOffPixel = 0x0000;   // backlight off / panel asleep
constexpr uint16_t kBezelPixel = 0x2104; // outside the round panel's glass

SDL_Window *g_window = nullptr;
SDL_Renderer *g_renderer = nullptr;
SDL_Texture *g_texture = nullptr;
char **g_argv = nullptr;

bool g_roundMask = true;
// True: show the panel upright, as someone holding the cube on the current
// face sees it. False: show the raw physical panel, which appears rotated.
bool g_userView = true;
bool g_sleeping = false;

// Set from SIM_SCREENSHOT / SIM_SCREENSHOT_MS: grab the panel at one or more
// elapsed times, then exit. Useful for eyeballing a UI change over SSH, or for
// capturing a countdown at several points without sitting and watching it.
const char *g_screenshotPath = nullptr;
std::vector<Uint32> g_screenshotTimes;
size_t g_screenshotIndex = 0;

// A voltage on each side of LOW_BATTERY_VOLTAGE, so the forced states go
// through the firmware's own threshold rather than around it.
constexpr float kForcedLowVoltage = 3.55f;
constexpr float kForcedHealthyVoltage = 3.90f;

const char *batteryStateName() {
  switch (SimInput::batteryOverride) {
    case SimInput::BatteryOverride::Warning: return "forced low";
    case SimInput::BatteryOverride::Healthy: return "forced ok";
    default: return nullptr;
  }
}

// Battery::cycleBatteryUpdate() repaints from the simulated voltage every five
// seconds, so an override has to be re-asserted after each pass of loop().
void applyBatteryOverride() {
  switch (SimInput::batteryOverride) {
    case SimInput::BatteryOverride::Warning:
      Display::updateBattery(kForcedLowVoltage);
      break;
    case SimInput::BatteryOverride::Healthy:
      Display::updateBattery(kForcedHealthyVoltage);
      break;
    case SimInput::BatteryOverride::None:
      break;
  }
}

// There is no radio here, so `a` prints what would go out instead. Handy for
// reading a payload back against the BTHome spec, or pasting into a decoder.
void dumpAdvertisement() {
  static uint8_t packetId = 0;

  const bool countingUp = timerMode == TimerMode::CountUp;

  BTHome::State state;
  state.packetId = packetId++;
  state.batteryVolts = SimInput::batteryVoltage;
  state.awake = true;
  // A stint counting up is running from its first second, before remSeconds has
  // anything in it.
  state.running = countingUp || remSeconds > 0;
  state.work = timerKind == TimerKind::Work;
  state.pomodoroCount = RtcState::data().pomodoroCount;
  state.remainingSeconds = remSeconds;
  state.selectedSeconds = selSeconds;

  uint8_t advert[BTHome::MAX_ADVERTISEMENT];
  const size_t length = BTHome::encode(state, advert, sizeof(advert));
  if (length == 0) {
    std::printf("[sim] advertisement would not fit\n");
    return;
  }

  std::printf("[sim] advertisement (%zu bytes):", length);
  for (size_t i = 0; i < length; i++) std::printf(" %02X", advert[i]);
  std::printf("\n");
}

const char *orientationName(Orientation ori) {
  switch (ori) {
    case Orientation::FACE_DOWN: return "face down";
    case Orientation::FACE_UP:   return "face up";
    case Orientation::DEG_0:   return "0deg";
    case Orientation::DEG_90:  return "90deg";
    case Orientation::DEG_180: return "180deg";
    case Orientation::DEG_270: return "270deg";
    default:                   return "?";
  }
}

void shutdown() {
  if (g_texture) SDL_DestroyTexture(g_texture);
  if (g_renderer) SDL_DestroyRenderer(g_renderer);
  if (g_window) SDL_DestroyWindow(g_window);
  SDL_Quit();
}

[[noreturn]] void quit() {
  shutdown();
  std::exit(0);
}

// A deep sleep on real hardware ends in a cold boot when the IMU interrupt
// fires, so wake is modelled by re-executing this process from scratch.
[[noreturn]] void reboot() {
  shutdown();
  ::setenv("SIM_WOKE_FROM_SLEEP", "1", 1);
  execv("/proc/self/exe", g_argv);
  std::perror("execv");
  std::exit(1);
}

void updateTitle() {
  char battery[32];
  const char *forced = batteryStateName();
  if (forced) std::snprintf(battery, sizeof(battery), "batt %s", forced);
  else std::snprintf(battery, sizeof(battery), "batt %.2fV", (double)SimInput::batteryVoltage);

  char title[192];
  std::snprintf(title, sizeof(title),
                "pomodoro-cube sim  |  %s  |  %s  |  bl %d%%  |  rot %u%s%s",
                g_sleeping ? "ASLEEP" : orientationName(SimInput::orientation),
                battery, SimPanel::backlightPercent, SimPanel::rotation(),
                SimInput::beeperActive ? "  |  BEEP" : "",
                g_userView ? "" : "  |  panel view");
  SDL_SetWindowTitle(g_window, title);
}

// SIM_ORIENTATION lets a script boot the sim onto a given face, which is what
// scripted screenshots of each timer length need.
void applyBatteryFromEnv() {
  const char *value = std::getenv("SIM_BATTERY");
  if (!value) return;
  SimInput::batteryVoltage = (float)std::atof(value);
}

void applyOrientationFromEnv() {
  const char *value = std::getenv("SIM_ORIENTATION");
  if (!value) return;

  const std::string name(value);
  if (name == "0") SimInput::orientation = Orientation::DEG_0;
  else if (name == "90") SimInput::orientation = Orientation::DEG_90;
  else if (name == "180") SimInput::orientation = Orientation::DEG_180;
  else if (name == "270") SimInput::orientation = Orientation::DEG_270;
  else if (name == "down" || name == "sleep") SimInput::orientation = Orientation::FACE_DOWN;
  else if (name == "up") SimInput::orientation = Orientation::FACE_UP;
  else std::fprintf(stderr, "[sim] unknown SIM_ORIENTATION '%s'\n", value);
}

// With one capture time the file is exactly SIM_SCREENSHOT; with several, each
// gets its elapsed time appended so the frames stay distinguishable.
std::string screenshotPathFor(size_t index) {
  const std::string path(g_screenshotPath);
  if (g_screenshotTimes.size() <= 1) return path;

  const size_t dot = path.find_last_of('.');
  const std::string suffix = "-" + std::to_string(g_screenshotTimes[index]);
  if (dot == std::string::npos) return path + suffix;
  return path.substr(0, dot) + suffix + path.substr(dot);
}

void saveScreenshot(const uint16_t *pixels, const std::string &path) {
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
      (void *)pixels, SimPanel::WIDTH, SimPanel::HEIGHT, 16,
      SimPanel::WIDTH * (int)sizeof(uint16_t), SDL_PIXELFORMAT_RGB565);
  if (!surface) {
    std::fprintf(stderr, "screenshot surface failed: %s\n", SDL_GetError());
    return;
  }
  if (SDL_SaveBMP(surface, path.c_str()) != 0) {
    std::fprintf(stderr, "screenshot save failed: %s\n", SDL_GetError());
  } else {
    std::printf("[sim] wrote %s\n", path.c_str());
  }
  SDL_FreeSurface(surface);
}

// A backlight at N% duty emits N% of the light, but these are sRGB code values,
// not light: scaling them by N directly renders far darker than the panel
// actually looks. Scale the *luminance* instead, which in code values is
// N^(1/2.2) -- 20% duty comes out around 48%, which is roughly what the eye
// reports. Still only an approximation of one panel's gamma, but close enough
// that a dim face on the desktop can be judged rather than merely spotted.
double backlightScale(int percent) {
  if (percent >= 100) return 1.0;
  if (percent <= 0) return 0.0;
  return std::pow(percent / 100.0, 1.0 / 2.2);
}

uint16_t dimmed(uint16_t pixel, double scale) {
  if (scale >= 1.0) return pixel;
  if (scale <= 0.0) return kOffPixel;

  const int red = (int)(((pixel >> 11) & 0x1F) * scale + 0.5);
  const int green = (int)(((pixel >> 5) & 0x3F) * scale + 0.5);
  const int blue = (int)((pixel & 0x1F) * scale + 0.5);
  return (uint16_t)((red << 11) | (green << 5) | blue);
}

void render() {
  static uint16_t scratch[SimPanel::WIDTH * SimPanel::HEIGHT];

  const double brightness = backlightScale(SimPanel::asleep ? 0 : SimPanel::backlightPercent);
  // Squared radius of the round panel's visible aperture.
  constexpr int r = SimPanel::WIDTH / 2;
  constexpr int rSquared = r * r;

  for (int y = 0; y < SimPanel::HEIGHT; y++) {
    for (int x = 0; x < SimPanel::WIDTH; x++) {
      const int i = y * SimPanel::WIDTH + x;
      if (g_roundMask) {
        const int dx = x - r;
        const int dy = y - r;
        if (dx * dx + dy * dy > rSquared) {
          scratch[i] = kBezelPixel;
          continue;
        }
      }
      // LVGL is built with LV_COLOR_16_SWAP, so the panel holds RGB565 in
      // big-endian byte order; swap it back for SDL's RGB565 texture.
      const int src = g_userView ? SimPanel::panelIndex(x, y) : i;
      scratch[i] = dimmed(__builtin_bswap16(SimPanel::framebuffer[src]), brightness);
    }
  }

  SDL_UpdateTexture(g_texture, nullptr, scratch, SimPanel::WIDTH * (int)sizeof(uint16_t));
  SDL_RenderClear(g_renderer);
  SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
  SDL_RenderPresent(g_renderer);
  SimPanel::dirty = false;

  while (g_screenshotPath && g_screenshotIndex < g_screenshotTimes.size() &&
         SDL_GetTicks() >= g_screenshotTimes[g_screenshotIndex]) {
    saveScreenshot(scratch, screenshotPathFor(g_screenshotIndex));
    g_screenshotIndex++;
  }
  if (g_screenshotPath && g_screenshotIndex >= g_screenshotTimes.size()) quit();
}

void handleKey(SDL_Keycode key) {
  switch (key) {
    case SDLK_1: SimInput::orientation = Orientation::DEG_0;   break;
    case SDLK_2: SimInput::orientation = Orientation::DEG_90;  break;
    case SDLK_3: SimInput::orientation = Orientation::DEG_180; break;
    case SDLK_4: SimInput::orientation = Orientation::DEG_270; break;
    case SDLK_0:
    case SDLK_s: SimInput::orientation = Orientation::FACE_DOWN; break;
    case SDLK_u: SimInput::orientation = Orientation::FACE_UP;   break;
    case SDLK_LEFTBRACKET:
      SimInput::batteryOverride = SimInput::BatteryOverride::None;
      SimInput::batteryVoltage -= 0.05f;
      if (SimInput::batteryVoltage < 3.0f) SimInput::batteryVoltage = 3.0f;
      break;
    case SDLK_RIGHTBRACKET:
      SimInput::batteryOverride = SimInput::BatteryOverride::None;
      SimInput::batteryVoltage += 0.05f;
      if (SimInput::batteryVoltage > 4.2f) SimInput::batteryVoltage = 4.2f;
      break;
    case SDLK_b:
      // Cycle so the voltage model is always reachable again.
      switch (SimInput::batteryOverride) {
        case SimInput::BatteryOverride::None:
          SimInput::batteryOverride = SimInput::BatteryOverride::Warning;
          break;
        case SimInput::BatteryOverride::Warning:
          SimInput::batteryOverride = SimInput::BatteryOverride::Healthy;
          break;
        case SimInput::BatteryOverride::Healthy:
          SimInput::batteryOverride = SimInput::BatteryOverride::None;
          break;
      }
      break;
    case SDLK_m: g_roundMask = !g_roundMask; SimPanel::dirty = true; break;
    case SDLK_v: g_userView = !g_userView; SimPanel::dirty = true; break;
    case SDLK_a: dumpAdvertisement(); break;
    case SDLK_r: reboot();
    case SDLK_q:
    case SDLK_ESCAPE: quit();
    default: break;
  }
}

// SIM_KEYS replays keys so a scripted run can reach states that otherwise need
// a keyboard. Entries are comma-separated; a bare key is pressed at startup and
// `u@12000` presses it 12s in, which is how a face change part-way through a
// countdown gets tested. Keycodes for the keys this sim uses are their ASCII
// values, so a key is taken literally.
struct ScheduledKey {
  Uint32 atMs;
  SDL_Keycode key;
};
std::vector<ScheduledKey> g_scheduledKeys;
size_t g_scheduledIndex = 0;

void applyKeysFromEnv() {
  const char *keys = std::getenv("SIM_KEYS");
  if (!keys) return;

  std::string spec(keys);
  size_t start = 0;
  while (start <= spec.size()) {
    const size_t comma = spec.find(',', start);
    const std::string piece = spec.substr(start, comma - start);
    if (!piece.empty()) {
      const size_t at = piece.find('@');
      const SDL_Keycode key = (SDL_Keycode)piece[0];
      if (at == std::string::npos) handleKey(key);
      else g_scheduledKeys.push_back({(Uint32)std::atoi(piece.c_str() + at + 1), key});
    }
    if (comma == std::string::npos) break;
    start = comma + 1;
  }

  std::sort(g_scheduledKeys.begin(), g_scheduledKeys.end(),
            [](const ScheduledKey &a, const ScheduledKey &b) { return a.atMs < b.atMs; });
}

void fireScheduledKeys() {
  while (g_scheduledIndex < g_scheduledKeys.size() &&
         SDL_GetTicks() >= g_scheduledKeys[g_scheduledIndex].atMs) {
    handleKey(g_scheduledKeys[g_scheduledIndex].key);
    g_scheduledIndex++;
  }
}

void pumpEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) quit();
    if (event.type == SDL_KEYDOWN && event.key.repeat == 0) handleKey(event.key.keysym.sym);
  }
}

void onDelay(unsigned long ms) {
  pumpEvents();
  fireScheduledKeys();
  render();
  updateTitle();
  SDL_Delay((Uint32)ms);
}

// Parked after a deep sleep: the panel is dark and only a keypress gets us out.
[[noreturn]] void waitForWake() {
  g_sleeping = true;
  std::printf("[sim] deep sleep -- press any key to wake (q to quit)\n");
  updateTitle();

  for (;;) {
    render();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) quit();
      if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_q || event.key.keysym.sym == SDLK_ESCAPE) quit();
        // Wake on a face that runs a timer, the way picking the cube up would.
        if (Util::isRestingFace(SimInput::orientation)) {
          SimInput::orientation = Orientation::DEG_90;
        }
        reboot();
      }
    }
    SDL_Delay(20);
  }
}

}  // namespace

int main(int argc, char **argv) {
  (void)argc;
  g_argv = argv;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  g_window = SDL_CreateWindow("pomodoro-cube sim", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SimPanel::WIDTH * kScale,
                              SimPanel::HEIGHT * kScale, SDL_WINDOW_SHOWN);
  if (!g_window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return 1;
  }

  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
  if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
  if (!g_renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return 1;
  }

  g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING, SimPanel::WIDTH,
                                SimPanel::HEIGHT);
  if (!g_texture) {
    std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    return 1;
  }

  g_screenshotPath = std::getenv("SIM_SCREENSHOT");
  if (g_screenshotPath) {
    const char *at = std::getenv("SIM_SCREENSHOT_MS");
    std::string spec(at ? at : "2000");
    size_t start = 0;
    while (start <= spec.size()) {
      const size_t comma = spec.find(',', start);
      const std::string piece = spec.substr(start, comma - start);
      if (!piece.empty()) g_screenshotTimes.push_back((Uint32)std::atoi(piece.c_str()));
      if (comma == std::string::npos) break;
      start = comma + 1;
    }
    std::sort(g_screenshotTimes.begin(), g_screenshotTimes.end());
  }

  applyOrientationFromEnv();
  applyBatteryFromEnv();
  applyKeysFromEnv();

  SimRtc::restore(std::getenv("SIM_WOKE_FROM_SLEEP") != nullptr);
  SimHost::delayHook = onDelay;

  std::printf(
      "[sim] keys: 1/2/3/4 = cube faces, 0 or s = face down, u = face up,\n"
      "      b = force low battery warning on/off, [ / ] = battery voltage,\n"
      "      v = user/panel view, m = round mask, a = print BLE advertisement,\n"
      "      r = reboot, q = quit\n");

  try {
    setup();
    for (;;) {
      pumpEvents();
      fireScheduledKeys();
      loop();
      applyBatteryOverride();
      if (SimPanel::dirty) render();
      updateTitle();
    }
  } catch (const SimDeepSleep &) {
    // RTC memory survives deep sleep on the device; persist it so the re-exec
    // comes back to the same block.
    SimRtc::persist();
    waitForWake();
  }
}
