#include "display.h"
#include "consts.h"
#include "indicators.h"
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
  // A paused sleep locks the backlight on through deep sleep; release it before
  // driving the pin again.
  gpio_hold_dis((gpio_num_t)TFT_BL_PIN);
  releaseBacklightPwm();
  digitalWrite(TFT_BL_PIN, HIGH);
  // Initialize TFT
  tft.begin();
  tft.setRotation(0);  // Let LVGL handle software rotation

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
  releaseBacklightPwm();
  digitalWrite(TFT_BL_PIN, LOW);
  // Send GC9A01 Sleep In command
  tft.writecommand(0x10);
  delay(120); // Required transition delay for the controller to power down
}

void Display::holdPausedFrame() {
  // No Sleep In command: the panel keeps refreshing the frame from its own
  // memory, and holding the pin keeps it lit once the CPU stops.
  //
  // Full brightness, not the idle level, and not a choice: nothing is left
  // running to generate PWM once the CPU stops, and gpio_hold_en() freezes the
  // instantaneous level rather than the duty cycle. A parked cube is therefore
  // lit at full or not at all -- turning it face down is how you turn it off.
  releaseBacklightPwm();
  digitalWrite(TFT_BL_PIN, HIGH);
  gpio_hold_en((gpio_num_t)TFT_BL_PIN);
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

void Display::rotateScreen(Orientation ori) {
  switch (ori) {
    case Orientation::DEG_0: tft.setRotation(0); break;
    case Orientation::DEG_90: tft.setRotation(1); break;
    case Orientation::DEG_180: tft.setRotation(2); break;
    case Orientation::DEG_270: tft.setRotation(3); break;
    default: tft.setRotation(0);
  }
  // No colour reset needed -- updateTimer() always follows a rotation and
  // repaints the arc from the remaining time.
  lv_obj_invalidate(lv_scr_act());
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
}

unsigned long lastFinishChange = 0;
bool finishInverted = false;

void Display::cycleTimerFinish() {
  const unsigned long now = millis();

  // Called every pass while the timer sits at zero, and self-timed: the beeper
  // blocks for the length of each note, so the flash cannot be hung off the
  // beep sequence without inheriting its cadence.
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