#pragma once

namespace QMI {

void setup();
void setupWakeup();
bool getAccelerometer(float &ax, float &ay, float &az);

// Turn on the QMI8658's own tap engine, which watches for the sharp peak and
// the quiet that follows it at the sensor's full sample rate. Doing this in
// software from the 50 Hz polling loop is not an option: a tap is over in a
// couple of milliseconds, so almost every one would fall between two samples.
void enableTapDetection();

// True once per tap. The sensor latches the event and clears it on read, so a
// tap between two calls is not missed and no tap is ever reported twice.
bool takeTap();

}
