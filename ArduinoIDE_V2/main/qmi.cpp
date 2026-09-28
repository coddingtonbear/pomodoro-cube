#include "qmi.h"
#include "SensorQMI8658.hpp"  // By Lewis He V0.4.1
#include "consts.h"

SensorQMI8658 qmi;

void QMI::setup() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // Initialize IMU
  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, I2C_SDA_PIN, I2C_SCL_PIN)) {
    Serial.println("QMI8658 initialization failed!");
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
  // as single taps. 0.25 is about 0.5 g. The quiet floor is still the
  // datasheet's example: lowering the peak and not the floor keeps a tap one
  // sharp jolt that dies away, which a hand setting the cube down is not.
  qmi.configTap(SensorQMI8658::PRIORITY0,  // x > y > z; the glass is the z face
                40,      // peakWindow:  20 @500Hz
                100,     // tapWindow:   50 @500Hz
                500,     // dTapWindow: 250 @500Hz
                0.0625f, // alpha
                0.25f,   // gamma
                0.25f,   // peakMagThr, g^2
                0.4f);   // UDMThr, g^2
  qmi.enableTap(SensorQMI8658::INTERRUPT_PIN_1);
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
  return qmi.getAccelerometer(ax, ay, az);
}