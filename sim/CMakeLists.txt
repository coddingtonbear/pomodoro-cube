# Desktop build of the cube firmware: the real main.ino, display, util,
# battery and beeper sources are compiled against the shims in shim/, with
# LVGL rendering into an SDL window instead of a GC9A01 over SPI.
cmake_minimum_required(VERSION 3.16)
project(pomodoro_cube_sim C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)
include(FetchContent)

# SOURCE_SUBDIR points at a directory with no CMakeLists.txt so that LVGL's own
# build is skipped; the sources are compiled into this project directly, which
# keeps lv_conf handling under our control.
FetchContent_Declare(
  lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v8.3.11
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-add-subdirectory
)
FetchContent_MakeAvailable(lvgl)

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS ${lvgl_SOURCE_DIR}/src/*.c)

set(FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../ArduinoIDE_V2/main)

# The SquareLine-generated UI, compiled verbatim.
set(UI_SOURCES
  ${FIRMWARE_DIR}/src/ui.c
  ${FIRMWARE_DIR}/src/ui_Screen1.c
  ${FIRMWARE_DIR}/src/ui_helpers.c
  ${FIRMWARE_DIR}/src/ui_comp_hook.c
  ${FIRMWARE_DIR}/src/ui_font_Countdown_54.c
)

# Firmware sources that are portable enough to host-compile. qmi.cpp is the one
# exception: it needs the vendor QMI8658 driver, so src/sim_qmi.cpp stands in.
set(FIRMWARE_SOURCES
  ${FIRMWARE_DIR}/main.ino
  ${FIRMWARE_DIR}/bthome.cpp
  ${FIRMWARE_DIR}/display.cpp
  ${FIRMWARE_DIR}/indicators.cpp
  ${FIRMWARE_DIR}/rtc_state.cpp
  ${FIRMWARE_DIR}/util.cpp
  ${FIRMWARE_DIR}/battery.cpp
  ${FIRMWARE_DIR}/beeper.cpp
)
# The Arduino IDE force-includes Arduino.h into every sketch translation unit
# and compiles .ino as C++; reproduce both so the firmware needs no edits.
set_source_files_properties(${FIRMWARE_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-include;Arduino.h")
set_source_files_properties(${FIRMWARE_DIR}/main.ino PROPERTIES
  LANGUAGE CXX
  COMPILE_OPTIONS "-x;c++;-include;Arduino.h")

add_executable(sim
  ${LVGL_SOURCES}
  ${UI_SOURCES}
  ${FIRMWARE_SOURCES}
  shim/Arduino.cpp
  shim/TFT_eSPI.cpp
  src/sim_panel.cpp
  src/sim_rtc.cpp
  src/sim_input.cpp
  src/sim_qmi.cpp
  src/sim_main.cpp
)

target_include_directories(sim PRIVATE
  ${lvgl_SOURCE_DIR}
  ${lvgl_SOURCE_DIR}/..
  ${FIRMWARE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/shim
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${SDL2_INCLUDE_DIRS}
)

# LV_CONF_SKIP makes LVGL fall back to its built-in defaults, which suit this
# project apart from the heap size; override further options here rather than
# carrying a 700-line lv_conf.h copy.
target_compile_definitions(sim PRIVATE
  LV_CONF_SKIP=1
  LV_COLOR_DEPTH=16
  # SquareLine Studio generates UI code that asserts this.
  LV_COLOR_16_SWAP=1
  LV_MEM_SIZE=65536U
)

target_link_libraries(sim PRIVATE ${SDL2_LIBRARIES})

# The generated UI and LVGL itself are third-party; keep their warnings out of
# the way so the firmware's own warnings stay visible.
set_source_files_properties(${LVGL_SOURCES} ${UI_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")

# --- Host tests ------------------------------------------------------------
# util.cpp's logic touches no LVGL, so the test binary skips it entirely and
# links against the same Arduino shim the simulator uses.
enable_testing()

add_executable(firmware_tests
  ${FIRMWARE_DIR}/bthome.cpp
  ${FIRMWARE_DIR}/indicators.cpp
  ${FIRMWARE_DIR}/rtc_state.cpp
  ${FIRMWARE_DIR}/rtc_state.cpp
  ${FIRMWARE_DIR}/util.cpp
  shim/Arduino.cpp
  src/sim_panel.cpp
  src/sim_rtc.cpp
  src/sim_input.cpp
  tests/stubs.cpp
  tests/test_util.cpp
  tests/test_indicators.cpp
  tests/test_rtc_state.cpp
  tests/test_bthome.cpp
  tests/main.cpp
)

set_source_files_properties(${FIRMWARE_DIR}/util.cpp ${FIRMWARE_DIR}/rtc_state.cpp
  PROPERTIES COMPILE_OPTIONS "-include;Arduino.h")

target_include_directories(firmware_tests PRIVATE
  ${FIRMWARE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/shim
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME firmware_tests COMMAND firmware_tests)
