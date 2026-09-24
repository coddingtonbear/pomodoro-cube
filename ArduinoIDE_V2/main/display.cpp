#include "display.h"
#include "consts.h"
#include "indicators.h"
#include <lvgl.h>
#include <Wire.h>
#include <TFT_eSPI.h>  // By Bodmer V2.5.43
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


void Display::setup() {
  // A paused sleep locks the backlight on through deep sleep; release it before
  // driving the pin again.
  gpio_hold_dis((gpio_num_t)TFT_BL_PIN);
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);
  // Initialize TFT
  tft.begin();
  tft.setRotation(0);  // Let LVGL handle software rotation

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

// Paint the panel's background, the digits and the unit marker as one palette,
// so the inversion can't half-apply.
static void setPalette(uint32_t background, uint32_t text, uint32_t track) {
  lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(background), LV_PART_MAIN);
  lv_obj_set_style_text_color(ui_Countdown, lv_color_hex(text), LV_PART_MAIN);
  lv_obj_set_style_text_color(ui_UnitMarker, lv_color_hex(text), LV_PART_MAIN);
  lv_obj_set_style_arc_color(ui_Arc1, lv_color_hex(track), LV_PART_MAIN);
}

void Display::deepSleep() {
  digitalWrite(TFT_BL_PIN, LOW);
  // Send GC9A01 Sleep In command
  tft.writecommand(0x10); 
  delay(120); // Required transition delay for the controller to power down
}

void Display::holdPausedFrame() {
  // No Sleep In command and no backlight change: the panel keeps refreshing the
  // frame from its own memory. Holding the pin keeps it lit once the CPU stops.
  gpio_hold_en((gpio_num_t)TFT_BL_PIN);
}

void Display::showPaused() {
  // Back to the dark palette whether the pause caught a countdown or a flow
  // stint: paused has to look like one thing.
  setArcAppearance(ARC_COLOR_PAUSED, LV_OPA_COVER);
  setPalette(SCREEN_BG_COLOR, COUNTDOWN_COLOR_PAUSED, ARC_TRACK_COLOR);

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

static void show(lv_obj_t *obj, bool visible) {
  if (visible) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void setCountdownText(int seconds) {
  const Indicators::ClockFields fields = Indicators::clockFields(seconds);
  lv_label_set_text_fmt(ui_Countdown, "%02d:%02d", fields.left, fields.right);
  show(ui_UnitMarker, fields.hours);
}

void Display::updateTimer(int seconds, int selSeconds, bool countingUp) {
  setCountdownText(seconds);

  if (countingUp) {
    // Flow mode: no total to drain against, so the arc fills as a lap indicator
    // and the panel inverts to black on white. Undoes showPaused() as the
    // countdown branch does, without needing to know whether it ran.
    const int lap = Indicators::lapPercent(seconds);
    lv_arc_set_value(ui_Arc1, lap);
    setArcAppearance(Indicators::flowArcColor(lap), LV_OPA_COVER);
    setPalette(FLOW_BG_COLOR, COUNTDOWN_COLOR_FLOW, FLOW_ARC_TRACK_COLOR);
    return;
  }

  // The countdown arc starts full and drains, shading green through amber to red.
  const int remaining = Indicators::remainingPercent(seconds, selSeconds);
  lv_arc_set_value(ui_Arc1, remaining);
  setArcAppearance(Indicators::arcColor(remaining), LV_OPA_COVER);
  setPalette(SCREEN_BG_COLOR, COUNTDOWN_COLOR, ARC_TRACK_COLOR);
}

unsigned long lastFinishChange = 0;
bool finishColorState = false;

void Display::cycleTimerFinish() {
  if (millis() - lastFinishChange < 800) return;

  // A drained arc has nothing left to flash, so refill it and pulse the ring
  // between red and a dark red instead.
  lv_arc_set_value(ui_Arc1, 100);
  setArcAppearance(finishColorState ? ARC_COLOR_LOW : ARC_COLOR_FINISH_DIM, LV_OPA_COVER);
  finishColorState = !finishColorState;
  lastFinishChange = millis();
}