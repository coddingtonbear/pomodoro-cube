#pragma once

// TFT_eSPI's configuration for the Waveshare ESP32-S3-Touch-LCD-1.28.
//
// This lives here rather than in the sketch folder as TFT_eSPI's `tft_setup.h`
// because that mechanism only works for the sketch's own translation units:
// TFT_eSPI.h looks for <tft_setup.h> on the include path, and the Arduino
// build does not put the sketch folder on the path when it compiles the
// library itself. The library half of the build therefore falls back to
// User_Setup.h -- a default ILI9341 on entirely different pins -- while the
// sketch half believes it is talking to a GC9A01, and the panel stays dark
// with nothing to say why. Overriding User_Setup.h instead means every
// translation unit is configured the same way.
//
// Symlink it into place; see the README's Building section.

#define USER_SETUP_INFO "Waveshare_128"
#define GC9A01_DRIVER
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

#define TFT_MISO -1
#define TFT_MOSI 11
#define TFT_SCLK 10
#define TFT_CS 9
#define TFT_DC 8
#define TFT_RST 14
#define TFT_BL 2

// Required by LVGL / TFT_eSPI bridge
#define LOAD_GLCD
#define LOAD_FONT2
#define SPI_FREQUENCY 27000000

#define USE_HSPI_PORT