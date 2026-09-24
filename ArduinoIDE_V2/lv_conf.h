/* LVGL configuration for the cube.
 *
 * Only the options that differ from LVGL's own defaults are set here --
 * lv_conf_internal.h fills in everything else -- so this file doesn't have to
 * be reconciled against a 700-line template every time LVGL adds an option.
 * sim/CMakeLists.txt sets the same three by hand, which is what keeps the
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

/* The default heap is too small for the 240x240 UI's objects and styles. */
#define LV_MEM_SIZE (64U * 1024U)

#endif /* LV_CONF_H */
