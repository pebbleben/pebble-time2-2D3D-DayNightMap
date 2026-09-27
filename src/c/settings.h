#pragma once
#include <pebble.h>

#define SETTINGS_KEY 3
#define MAP_PROJECTION_2D 0
#define MAP_PROJECTION_3D 1

/* Preserve the supplied packed layout; append new fields only. */
typedef struct ClaySettings {
  GColor BackgroundColor;
  GColor ForegroundColor;
  int DayMapIndex;
  int NightMapIndex;
  int LongitudeOffset;
  int LatitudeOffset;
  int CenterFocus;
  GColor DayLand;
  GColor DayWater;
  GColor DayIce;
  GColor NightLand;
  GColor NightWater;
  GColor NightIce;
  char TimeFont[32];
  char DateFont[32];
  int MapProjection;
} __attribute__((__packed__)) ClaySettings;

void settings_init(void);
ClaySettings *settings_get(void);
void settings_save(void);
bool settings_update_from_dict(DictionaryIterator *iter);