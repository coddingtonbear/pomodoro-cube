// Minimal Arduino / ESP32 API shim so the firmware can be compiled and run
// on a desktop host. Only the surface actually used by this project is
// provided -- anything else is deliberately absent so that a new hardware
// dependency shows up as a compile error rather than silently doing nothing.
#pragma once

#include <cstdint>
#include <cstddef>

#define HIGH 1
#define LOW 0
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05

// On the device this places a variable in RTC memory, which survives deep
// sleep. The simulator has no such memory: sim_rtc.cpp persists the block to a
// file across the simulated cold boot instead.
#define RTC_DATA_ATTR

// ---- ESP32 GPIO numbering -------------------------------------------------
enum gpio_num_t {
  GPIO_NUM_0 = 0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4,
  GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9,
  GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14,
  GPIO_NUM_15, GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_19,
  GPIO_NUM_20, GPIO_NUM_21,
};

// ---- Time -----------------------------------------------------------------
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);

// ---- GPIO -----------------------------------------------------------------
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
uint32_t analogReadMilliVolts(uint8_t pin);

// ---- LEDC (backlight PWM) -------------------------------------------------
// The firmware picks its LEDC calls on this, and arduino-esp32 3.x is what the
// board is built against; the shim only implements that side of the branch.
#define ESP_ARDUINO_VERSION_MAJOR 3
bool ledcAttach(uint8_t pin, uint32_t frequency, uint8_t resolution);
bool ledcWrite(uint8_t pin, uint32_t duty);
bool ledcDetach(uint8_t pin);


// ---- ESP32 power management ----------------------------------------------
bool setCpuFrequencyMhz(uint32_t mhz);
uint32_t getCpuFrequencyMhz();

// The heap figures the soak trace prints. The host has no heap worth
// reporting, so they read as zero.
struct EspClass {
  uint32_t getFreeHeap() { return 0; }
  uint32_t getMaxAllocHeap() { return 0; }
};
static EspClass ESP;
void gpio_hold_en(gpio_num_t pin);
void gpio_hold_dis(gpio_num_t pin);
void gpio_deep_sleep_hold_en();
void esp_sleep_enable_ext0_wakeup(gpio_num_t pin, int level);
[[noreturn]] void esp_deep_sleep_start();

// ---- Watchdog and reset reason --------------------------------------------
// The sim has no watchdog to arm, and every run of it is a power-on.
void enableLoopWDT();
void feedLoopWDT();
typedef enum { ESP_RST_UNKNOWN, ESP_RST_POWERON } esp_reset_reason_t;
esp_reset_reason_t esp_reset_reason();

// ---- Serial ---------------------------------------------------------------
class SimSerial {
public:
  void begin(unsigned long baud);
  void print(const char *s);
  void println(const char *s);
  void printf(const char *fmt, ...);
};
extern SimSerial Serial;
