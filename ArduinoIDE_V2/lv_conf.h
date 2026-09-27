/* LVGL configuration for the cube.
 *
 * Only the options that differ from LVGL's own defaults are set here --
 * lv_conf_internal.h fills in everything else -- so this file doesn't have to
 * be reconciled against a 700-line template every time LVGL adds an option.
 * sim/CMakeLists.txt sets the same four by hand, which is what keeps the
 * simulator and the board rendering from identical settings.
 *
 * LVGL looks for this file one directory above the library, so it has to be
 * copied to ~/Arduino/libraries/lv_conf.h; see the README's Building section.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16

/* The GC9A01 takes big-endian pixels, and the SquareLine-generated UI asserts
 * this is set. */
#define LV_COLOR_16_SWAP 1

/* The default heap is too small for the 240x240 UI's objects and styles, and
 * 64 KB was too small for a face drawn at an angle: the layer the countdown is
 * turned in runs to 42 KB on its own, LVGL asks for it in one piece, and it
 * does not check whether it got it. In the simulator that was a crash on the
 * third quarter turn. */
#define LV_MEM_SIZE (96U * 1024U)

/* Anything drawn at an angle is drawn flat into a layer first and turned on the
 * way to the screen, and the layer needs an alpha channel for whatever of it
 * the object does not cover. Without this LVGL declines to make the layer, and
 * the labels vanish for as long as the face is turned off square. */
#define LV_COLOR_SCREEN_TRANSP 1

#endif /* LV_CONF_H */
