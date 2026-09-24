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
  // datasheet's 500 Hz figures double at the 1000 Hz this runs at. The
  // thresholds are in g^2 and are the vendor's: a peak of 0.8 and a quiet floor
  // of 0.4 pick out a knuckle on the glass without firing on the desk being
  // knocked.
  qmi.configTap(SensorQMI8658::PRIORITY0,  // x > y > z; the glass is the z face
                40,      // peakWindow:  20 @500Hz
                100,     // tapWindow:   50 @500Hz
                500,     // dTapWindow: 250 @500Hz
                0.0625f, // alpha
                0.25f,   // gamma
                0.8f,    // peakMagThr, g^2
                0.4f);   // UDMThr, g^2
  qmi.enableTap(SensorQMI8658::INTERRUPT_PIN_1);
}

bool QMI::takeTap() {
  // Polled rather than wired to the interrupt. INT1 belongs to the wake-on-
  // motion configuration the sleep path installs, and while the cube is awake
  // the loop is already talking to the sensor every 20 ms anyway. update()
  // reads and clears the latched status, so the event is consumed here.
  if ((qmi.update() & SensorQMI8658::STATUS1_TAP_MOTION) == 0) return false;

  // Single and double taps arrive through the same event bit; the tap status
  // register is what tells them apart. Either counts, because what was asked
  // for is a tap -- narrowing this to DOUBLE_TAP is the whole change if single
  // taps turn out to fire at things that were not taps.
  const SensorQMI8658::TapEvent event = qmi.getTapStatus();
  return event == SensorQMI8658::SINGLE_TAP || event == SensorQMI8658::DOUBLE_TAP;
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