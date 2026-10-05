#include "qmi.h"
#include "SensorQMI8658.hpp"  // By Lewis He V0.4.1
#include "consts.h"

SensorQMI8658 qmi;

// Whether the tap engine is meant to be on, so a recovery can put it back.
static bool tapEnabled = false;

// The QMI8658 runs off the battery rail, so it keeps its power through every
// reset of the ESP32 -- the watchdog, the EN line, the RTS pulse after a flash.
// A reset that lands in the middle of a read leaves the sensor part-way through
// sending a byte, holding SDA low and waiting for clocks that never come. Every
// transaction after that fails, and on the board several reboots in a row came
// up with the sensor still silent. So before the bus is opened, clock the sensor out of whatever
// it was sending -- at most nine pulses, a byte and its ack -- and finish with
// a STOP, which is the standard I2C bus recovery.
static void recoverBus() {
  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SCL_PIN, HIGH);
  delayMicroseconds(10);
  const int sdaBefore = digitalRead(I2C_SDA_PIN);
  int pulses = 0;
  while (digitalRead(I2C_SDA_PIN) == LOW && pulses < 9) {
    digitalWrite(I2C_SCL_PIN, LOW);
    delayMicroseconds(10);
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(10);
    pulses++;
  }
  // STOP: SDA rising while SCL is high.
  pinMode(I2C_SDA_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SDA_PIN, LOW);
  delayMicroseconds(10);
  digitalWrite(I2C_SDA_PIN, HIGH);
  delayMicroseconds(10);
  Serial.printf("[trace] I2C recover sdaBefore=%d pulses=%d sdaAfter=%d scl=%d\n", sdaBefore,
                pulses, digitalRead(I2C_SDA_PIN), digitalRead(I2C_SCL_PIN));
}

void QMI::setup() {
  // A few tries, each from a recovered bus: the sensor also answers its soft
  // reset, which begin() issues, so a second attempt starts it from scratch.
  bool up = false;
  for (int attempt = 1; attempt <= 3 && !up; attempt++) {
    Wire.end();
    recoverBus();
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    up = qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, I2C_SDA_PIN, I2C_SCL_PIN);
    if (!up) Serial.printf("QMI8658 initialization failed (attempt %d)\n", attempt);
    // Each failed attempt can spend the better part of a second in the
    // library's reset wait; three of them must not trip the loop watchdog.
    feedLoopWDT();
  }

  qmi.configAccelerometer(
    SensorQMI8658::ACC_RANGE_4G,
    SensorQMI8658::ACC_ODR_1000Hz,
    SensorQMI8658::LPF_MODE_0);

  qmi.enableAccelerometer();
}

void QMI::enableTapDetection() {
  // Windows are counted in samples at the configured accelerometer ODR, so the
  // datasheet's 500 Hz figures double at the 1000 Hz this runs at.
  //
  // The thresholds are squared linear acceleration, in g^2. The datasheet's
  // example peak of 0.8 -- about 0.9 g -- is a firm knock for a cube lying on a
  // desk: on the board it let through two of a dozen double taps on the glass,
  // as single taps. 0.25 -- about 0.5 g -- was better, and still missed taps
  // now and then. 0.15 is about 0.4 g. The quiet floor is still the
  // datasheet's example: lowering the peak and not the floor keeps a tap one
  // sharp jolt that dies away, which a hand setting the cube down is not.
  qmi.configTap(SensorQMI8658::PRIORITY0,  // x > y > z; the glass is the z face
                40,      // peakWindow:  20 @500Hz
                100,     // tapWindow:   50 @500Hz
                500,     // dTapWindow: 250 @500Hz
                0.0625f, // alpha
                0.25f,   // gamma
                0.15f,   // peakMagThr, g^2
                0.4f);   // UDMThr, g^2
  qmi.enableTap(SensorQMI8658::INTERRUPT_PIN_1);
  tapEnabled = true;
}

QMI::Tap QMI::takeTap() {
  // Polled rather than wired to the interrupt. INT1 belongs to the wake-on-
  // motion configuration the sleep path installs, and while the cube is awake
  // the loop is already talking to the sensor every 20 ms anyway. update()
  // reads and clears the latched status, so the event is consumed here.
  if ((qmi.update() & SensorQMI8658::STATUS1_TAP_MOTION) == 0) return Tap::None;

  // Single and double taps arrive through the same event bit; the tap status
  // register is what tells them apart.
  switch (qmi.getTapStatus()) {
    case SensorQMI8658::SINGLE_TAP: return Tap::Single;
    case SensorQMI8658::DOUBLE_TAP: return Tap::Double;
    default: return Tap::None;
  }
}

void QMI::setupWakeup() {
  // Before the wake-on-motion configuration, so the tap engine cannot leave
  // INT1 asserting for something that is not motion. A tap is for brightening a
  // panel that is being looked at; there is nothing for it to do once the cube
  // has been put away. configWakeOnMotion() opens with a full sensor reset that
  // would clear this anyway; it is here so the intent survives a library
  // version that stops doing that.
  qmi.disableTap();
  tapEnabled = false;

  qmi.configWakeOnMotion(
    WAKE_ON_MOTION_THRESHOLD_MG,            // WoMThreshold, in milli-g
    SensorQMI8658::ACC_ODR_LOWPOWER_128Hz,  // Energy efficient frequency
    SensorQMI8658::INTERRUPT_PIN_1,         // Interrupt pin 1
    0                                       // INT1 idles low and rises on motion
  );
  float dummyX, dummyY, dummyZ;
  qmi.getAccelerometer(dummyX, dummyY, dummyZ);
}

bool QMI::getAccelerometer(float &ax, float &ay, float &az) {
  // The sensor can wedge while the cube is running, too -- seen as a timer
  // that counted on at a crawl and ignored being turned, every read failing. A run of failures long enough not to be
  // a glitch gets the same recovery as a boot, no more often than every few
  // seconds so a sensor that stays gone does not stall every pass.
  static uint32_t failures = 0;
  static unsigned long lastRecovery = 0;
  if (qmi.getAccelerometer(ax, ay, az)) {
    failures = 0;
    return true;
  }
  ax = ay = az = 0;
  if (++failures >= QMI_RECOVER_AFTER_FAILURES &&
      millis() - lastRecovery >= QMI_RECOVER_INTERVAL_MS) {
    lastRecovery = millis();
    Serial.printf("[trace] QMI recover t=%lu failures=%u\n", millis(), (unsigned)failures);
    failures = 0;
    setup();
    if (tapEnabled) enableTapDetection();
  }
  return false;
}