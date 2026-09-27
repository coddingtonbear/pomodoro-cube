#include "display.h"
#include "consts.h"
#include "indicators.h"
#include "tilt.h"
#include <lvgl.h>
#include <Wire.h>
#include <TFT_eSPI.h>  // By Bodmer V2.5.43

// A TFT_eSPI configured for something other than this panel compiles perfectly
// and then shows nothing at all, which is a miserable thing to debug. Fail at
// build time instead: if this is not set, ArduinoIDE_V2/User_Setup.h has not
// been symlinked into the library. See the README's Building section.
#ifndef GC9A01_DRIVER
#error "TFT_eSPI is not configured for the GC9A01 -- symlink ArduinoIDE_V2/User_Setup.h into ~/Arduino/libraries/TFT_eSPI/"
#endif
#include "src/ui.h"    // SquareLine Studio generated header


TFT_eSPI tft = TFT_eSPI();

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[240 * 20];
lv_disp_drv_t disp_drv;

void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, false);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}


namespace {

// Well above hearing, so the backlight never sings, and well within what the
// LED's series resistor and the switching FET will follow.
constexpr int kBacklightFrequency = 5000;
constexpr int kBacklightBits = 8;
// Only used by the 2.x API, which allocates channels by hand. tone() takes
// channel 0, so stay at the other end.
constexpr int kBacklightChannel = 7;

bool backlightIsPwm = false;
int backlightLevel = -1;

void attachBacklightPwm() {
  if (backlightIsPwm) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(TFT_BL_PIN, kBacklightFrequency, kBacklightBits);
#else
  ledcSetup(kBacklightChannel, kBacklightFrequency, kBacklightBits);
  ledcAttachPin(TFT_BL_PIN, kBacklightChannel);
#endif
  backlightIsPwm = true;
}

// Hand the pin back to plain GPIO so a static level can be held through deep
// sleep. Callers set that level themselves; this only gives them the pin.
void releaseBacklightPwm() {
  if (backlightIsPwm) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcDetach(TFT_BL_PIN);
#else
    ledcDetachPin(TFT_BL_PIN);
#endif
    backlightIsPwm = false;
  }
  pinMode(TFT_BL_PIN, OUTPUT);
  backlightLevel = -1;
}

// Give back the two pads holdPausedFrame() latched. Until this runs, nothing
// written to either reaches the pin: gpio_hold_en() survives deep sleep and
// keeps holding after the wake, which is the point of it. Safe when nothing was
// held, so every path that is about to drive the backlight or the reset line
// calls it first rather than reasoning about which sleep it woke from.
void releaseHeldPins() {
  gpio_hold_dis((gpio_num_t)TFT_BL_PIN);
  gpio_hold_dis((gpio_num_t)TFT_RST);
}

// Whether tft.begin() has run this boot. A wake that decides it should go
// straight back to sleep never calls Display::setup(), and still has to be able
// to put the panel away.
bool panelBegun = false;

void beginPanelOnce() {
  if (panelBegun) return;
  tft.begin();
  tft.setRotation(0);  // Let LVGL handle software rotation
  panelBegun = true;
}

// The face last drawn, kept so a change of brightness can repaint it in the
// other scheme without the caller having to hand the timer over again.
int lastRampAt = 100;
bool lastFlow = false;
bool dimScheme = false;

// A finished timer owns the panel until something ends it, so the ordinary
// repaints stand aside rather than fighting the flash for the background.
bool alerting = false;

// Defined below, once there is an applyPalette() for it to call.
void repaintPalette();

// How the face is turned: the quarter turn the panel is set to, and the whole
// degrees LVGL is drawing on top of it.
int panelQuarter = 0;
int lean = 0;

constexpr lv_coord_t kPanelCentre = 120;

// Turn everything on the face by the lean, about the centre of the panel.
//
// The arc turns itself, by starting its sweep somewhere else. Everything else
// is drawn flat into a layer and turned on the way to the screen, about a pivot
// LVGL measures from the object's own corner -- so the pivot is the centre of
// the panel restated for each object, and has to be restated whenever one
// changes size, which a label does with its text.
void applyLean() {
  lv_arc_set_rotation(ui_Arc1, (uint16_t)((lean + 360) % 360));

  lv_obj_update_layout(ui_Screen1);
  lv_obj_t *const leaning[] = {ui_LowBattery, ui_LowBatteryTip, ui_Countdown, ui_UnitMarker,
                               ui_BankLabel};
  for (lv_obj_t *obj : leaning) {
    const lv_coord_t pivotX = kPanelCentre - obj->coords.x1;
    const lv_coord_t pivotY = kPanelCentre - obj->coords.y1;
    // Each of these invalidates the object whether or not it changed anything,
    // and most passes change only the angle.
    if (lv_obj_get_style_transform_pivot_x(obj, LV_PART_MAIN) != pivotX) {
      lv_obj_set_style_transform_pivot_x(obj, pivotX, LV_PART_MAIN);
    }
    if (lv_obj_get_style_transform_pivot_y(obj, LV_PART_MAIN) != pivotY) {
      lv_obj_set_style_transform_pivot_y(obj, pivotY, LV_PART_MAIN);
    }
    if (lv_obj_get_style_transform_angle(obj, LV_PART_MAIN) != lean * 10) {
      lv_obj_set_style_transform_angle(obj, (lv_coord_t)(lean * 10), LV_PART_MAIN);
    }
  }
}

// For whatever has just changed the size of something on a leaning face. Does
// nothing on a square one, which is every face at rest.
void refreshLean() {
  if (lean != 0) applyLean();
}

}  // namespace

void Display::setBacklight(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  if (percent == backlightLevel) return;

  attachBacklightPwm();
  const uint32_t duty = (uint32_t)((percent * 255 + 50) / 100);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(TFT_BL_PIN, duty);
#else
  ledcWrite(kBacklightChannel, duty);
#endif
  backlightLevel = percent;

  // Dropping the backlight also swaps the scheme: at this brightness a field of
  // colour reads where a thin arc does not. Repaint only on the crossing, so
  // the every-20ms call in loop() stays free.
  const bool dim = percent < BACKLIGHT_FULL_PERCENT;
  if (dim != dimScheme) {
    dimScheme = dim;
    repaintPalette();
  }
}

void Display::setup() {
  // A paused sleep locks the backlight and the panel's reset line on through
  // deep sleep; release both before driving them again, or tft.begin() cannot
  // reset the panel.
  releaseHeldPins();
  releaseBacklightPwm();
  digitalWrite(TFT_BL_PIN, HIGH);
  // Initialize TFT
  beginPanelOnce();

  // PWM only once TFT_eSPI has finished with the pin. It leaves TFT_BL alone
  // unless TFT_BACKLIGHT_ON is defined, which tft_setup.h does not define -- but
  // an LEDC channel it did clobber would be silently stuck, and attaching after
  // begin() costs nothing to rule that out.
  Display::setBacklight(BACKLIGHT_FULL_PERCENT);

  // Initialize LVGL
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, 240 * 20);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 240;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;

  lv_disp_drv_register(&disp_drv);

  ui_init();
}


void Display::updateBattery(float voltage) {
  // Printed with %d rather than %.2f: lv_snprintf only handles floats when
  // LV_SPRINTF_USE_FLOAT is set, which is off by default and lives in an
  // lv_conf.h this repo doesn't control.
  const int centivolts = (int)(voltage * 100.0f + 0.5f);
  lv_label_set_text_fmt(ui_LowBatteryVoltage, "%d.%02d", centivolts / 100, centivolts % 100);

  // Nothing on screen at all until the charge is actually worth acting on.
  const bool warn = Indicators::showLowBattery(voltage);
  lv_obj_t *const parts[] = {ui_LowBattery, ui_LowBatteryTip};
  for (lv_obj_t *part : parts) {
    if (warn) lv_obj_clear_flag(part, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
  }
  refreshLean();
}

// Paint the arc and its knob in one colour, at one opacity.
static void setArcAppearance(uint32_t color, lv_opa_t opa) {
  const lv_color_t c = lv_color_hex(color);
  lv_obj_set_style_arc_color(ui_Arc1, c, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(ui_Arc1, opa, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(ui_Arc1, c, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(ui_Arc1, opa, LV_PART_KNOB);
}

// The low-battery outline is three objects with three different style
// properties, which is why it needs its own pass rather than joining the labels.
static void setBatteryColor(uint32_t color) {
  const lv_color_t c = lv_color_hex(color);
  lv_obj_set_style_border_color(ui_LowBattery, c, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_LowBatteryTip, c, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui_LowBatteryVoltage, c, LV_PART_MAIN);
}

static void show(lv_obj_t *obj, bool visible) {
  if (visible) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// Paint every colour on the panel at once, so a swap can't half-apply.
static void applyPalette(const Indicators::Palette &p) {
  lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(p.background), LV_PART_MAIN);
  lv_obj_set_style_arc_color(ui_Arc1, lv_color_hex(p.track), LV_PART_MAIN);
  lv_obj_t *const labels[] = {ui_Countdown, ui_UnitMarker, ui_BankLabel};
  for (lv_obj_t *label : labels) {
    lv_obj_set_style_text_color(label, lv_color_hex(p.text), LV_PART_MAIN);
  }
  setArcAppearance(p.arc, LV_OPA_COVER);
  setBatteryColor(p.battery);
}

namespace {

void repaintPalette() {
  if (alerting) return;
  applyPalette(Indicators::palette(lastRampAt, lastFlow, dimScheme));
}

}  // namespace

void Display::deepSleep() {
  // Both of these were no-ops when the last sleep held a frame: the pads were
  // still latched, so the backlight stayed on at full for the whole of the next
  // sleep. A cube parked face up and then turned face down went dark on the
  // outside and burned the pack on the inside.
  releaseHeldPins();
  releaseBacklightPwm();
  digitalWrite(TFT_BL_PIN, LOW);
  // Reached on a wake that went straight back to sleep without ever bringing
  // the display up, in which case there is no SPI to send a command over yet --
  // and a panel left holding a frame is still refreshing one, at a few
  // milliamps, which is an order above what the rest of the cube draws asleep.
  // The backlight is already off, so none of this is seen.
  beginPanelOnce();
  // Send GC9A01 Sleep In command
  tft.writecommand(0x10);
  delay(120); // Required transition delay for the controller to power down
}

void Display::holdPausedFrame() {
  // No Sleep In command: the panel keeps refreshing the frame from its own
  // memory, and holding the pin keeps it lit once the CPU stops.
  //
  // The reset line has to be held too. esp_deep_sleep_start() returns every pad
  // that is not held to its default -- high impedance -- and a floating RST on
  // the GC9A01 drifts low within seconds, which resets the controller, clears
  // its frame memory and drops it back into sleep-in. That is what blanked the
  // paused frame a few seconds after the cube was set down: the backlight was
  // still on, there was just nothing left on the panel to light.
  //
  // Full brightness, not the idle level, and not a choice: nothing is left
  // running to generate PWM once the CPU stops, and gpio_hold_en() freezes the
  // instantaneous level rather than the duty cycle. A parked cube is therefore
  // lit at full or not at all -- turning it face down is how you turn it off.
  // Deliberately does *not* release the holds first. When they are already on,
  // they are already holding exactly the levels this function is about to ask
  // for, so releasing them buys nothing -- and it hands the reset line back to
  // an output register that waking from deep sleep has cleared to zero. The pad
  // then presents a LOW for as long as it takes to get round to writing it
  // again, which is a reset pulse: the GC9A01 clears its frame memory and drops
  // into sleep-in, and the backlight goes on being held up over a blank panel.
  // That is the same symptom the held reset line was introduced to fix, put
  // back by the release that was supposed to make this path safe.
  releaseBacklightPwm();
  digitalWrite(TFT_BL_PIN, HIGH);
  gpio_hold_en((gpio_num_t)TFT_BL_PIN);

  // Level before direction, for the same reason: this writes the output
  // register while the pad is still an input, so changing the direction takes
  // it straight to HIGH rather than through whatever the register held.
  digitalWrite(TFT_RST, HIGH);
  pinMode(TFT_RST, OUTPUT);
  gpio_hold_en((gpio_num_t)TFT_RST);
}

void Display::showPaused() {
  // Back to the dark palette whether the pause caught a countdown or a flow
  // stint, and whether the panel was dim or bright: paused has to look like one
  // thing. A parked frame is always held at full brightness, so the dim scheme
  // has no business here -- and neither does a half-finished flash, which would
  // otherwise be the frame left lit on the panel for as long as the cube sits
  // there.
  alerting = false;
  show(ui_Arc1, true);
  applyPalette({SCREEN_BG_COLOR, COUNTDOWN_COLOR_PAUSED, ARC_COLOR_PAUSED, ARC_TRACK_COLOR,
                LOW_BATTERY_COLOR});

  // The frame has to reach the panel before the CPU stops, so pump LVGL rather
  // than waiting for the next loop() that will never come.
  for (int i = 0; i < 4; i++) {
    lv_timer_handler();
    lv_tick_inc(20);
    delay(20);
  }
}

void Display::setAngle(float degrees) {
  const Tilt::Split split = Tilt::split(degrees);

  if (split.quarter != panelQuarter) {
    panelQuarter = split.quarter;
    // Changes where writes land, not what is already on the glass, so the old
    // frame stands until the new one is drawn over it.
    tft.setRotation(split.quarter);
    lv_obj_invalidate(lv_scr_act());
  }

  if (split.lean != lean) {
    lean = split.lean;
    applyLean();
  }
}

void Display::rotateScreen(Orientation ori) {
  Display::setAngle(Tilt::faceAngle(ori));
}

static void setCountdownText(int seconds) {
  const Indicators::ClockFields fields = Indicators::clockFields(seconds);
  lv_label_set_text_fmt(ui_Countdown, "%02d:%02d", fields.left, fields.right);
  show(ui_UnitMarker, fields.hours);
}

// The bank in the smallest form that stays unambiguous: no marker to explain,
// because minutes and seconds are shown as such and hours only appear when
// there are some.
static void setBankText(int seconds) {
  if (seconds < 0) seconds = 0;
  if (seconds >= 3600) {
    lv_label_set_text_fmt(ui_BankLabel, "BANK %d:%02d:%02d", seconds / 3600,
                          (seconds % 3600) / 60, seconds % 60);
  } else {
    lv_label_set_text_fmt(ui_BankLabel, "BANK %d:%02d", seconds / 60, seconds % 60);
  }
}

void Display::updateTimer(const TimerView &view) {
  // Whatever brought us here ended the alarm: a new face, or a timer with
  // seconds on it again. Take the ring back before anything is painted.
  alerting = false;
  show(ui_Arc1, true);

  setCountdownText(view.seconds);

  // The bank belongs on the work face: on the break face the big number already
  // is the bank, counting down.
  const bool showBank = view.flow && view.countingUp;
  if (showBank) setBankText(view.bankSeconds);
  show(ui_BankLabel, showBank);

  // Counting up has no total to drain against, so the arc fills as a lap
  // indicator, its colour running the ramp over what is left of the lap.
  const int remaining = view.countingUp ? Indicators::lapPercent(view.seconds)
                                        : Indicators::remainingPercent(view.seconds, view.selSeconds);
  lastRampAt = view.countingUp ? 100 - remaining : remaining;
  lastFlow = view.flow;
  lv_arc_set_value(ui_Arc1, remaining);

  // Repaints every colour from scratch, which also undoes showPaused() without
  // needing to know whether it ran.
  repaintPalette();

  refreshLean();
}

unsigned long lastFinishChange = 0;
bool finishInverted = false;

void Display::cycleTimerFinish() {
  const unsigned long now = millis();

  // Called every pass while the timer sits at zero, and self-timed, so the
  // flash keeps its own cadence rather than the motor's.
  if (alerting) {
    if (now - lastFinishChange < (unsigned long)ALERT_FLASH_MS) return;
    finishInverted = !finishInverted;
  } else {
    // First pass of an alarm: take the panel, and start on the dark half so the
    // flash reads as something arriving rather than something leaving.
    alerting = true;
    finishInverted = false;
    show(ui_Arc1, false);
  }

  applyPalette(Indicators::alertPalette(finishInverted));
  lastFinishChange = now;
}