#pragma once
#include <pebble.h>

// The layout is designed against a 144x168 screen and scaled from the real
// root-layer bounds at runtime.
#define LAYOUT_H    168

#if PBL_DISPLAY_HEIGHT >= 200
  #define MAP_TOP_168 58   // emery / Time 2 -- globe pushed down toward the date
#else
  #define MAP_TOP_168 50   // 144x168 and 180x180
#endif

#define MAP_H_168   72

#define BATTERY_FONT FONT_KEY_GOTHIC_18