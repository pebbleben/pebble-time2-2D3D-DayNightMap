#include <pebble.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

#define SETTINGS_MAGIC 0x45525448u
#define SETTINGS_VERSION 1
static ClaySettings s_settings;
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  ClaySettings value;
} __attribute__((__packed__)) SavedSettings;

static int clamp_int(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
static void defaults(void) {
  memset(&s_settings, 0, sizeof(s_settings));
  s_settings.BackgroundColor = GColorBlack;
  s_settings.ForegroundColor = GColorWhite;
  s_settings.DayMapIndex = 2;
  s_settings.NightMapIndex = 2;
  s_settings.DayLand = GColorGreen;
  s_settings.DayWater = GColorBlue;
  s_settings.DayIce = GColorWhite;
  s_settings.NightLand = GColorDarkGreen;
  s_settings.NightWater = GColorOxfordBlue;
  s_settings.NightIce = GColorDarkGray;
  strcpy(s_settings.TimeFont, "leco");
  strcpy(s_settings.DateFont, "bitham");
  s_settings.MapProjection = MAP_PROJECTION_2D;
}
static void validate(void) {
  s_settings.DayMapIndex = clamp_int(s_settings.DayMapIndex, 0, 2);
  s_settings.NightMapIndex = clamp_int(s_settings.NightMapIndex, 0, 2);
  s_settings.CenterFocus = clamp_int(s_settings.CenterFocus, 0, 2);
  s_settings.LongitudeOffset = clamp_int(s_settings.LongitudeOffset, -180, 180);
  s_settings.LatitudeOffset = clamp_int(s_settings.LatitudeOffset, -90, 90);
  s_settings.MapProjection = clamp_int(s_settings.MapProjection, 0, 1);
  s_settings.TimeFont[sizeof(s_settings.TimeFont)-1] = '\0';
  s_settings.DateFont[sizeof(s_settings.DateFont)-1] = '\0';
}
void settings_init(void) {
  defaults();
  int size = persist_get_size(SETTINGS_KEY);
  if (size == (int)sizeof(SavedSettings)) {
    SavedSettings saved;
    if (persist_read_data(SETTINGS_KEY, &saved, sizeof(saved)) == (int)sizeof(saved)
        && saved.magic == SETTINGS_MAGIC && saved.version == SETTINGS_VERSION
        && saved.size == sizeof(ClaySettings)) {
      s_settings = saved.value;
    }
  } else {
    /* Migrate supplied raw structs, with or without LatitudeOffset. */
    const size_t with_lat = offsetof(ClaySettings, MapProjection);
    const size_t without_lat = with_lat - sizeof(int);
    uint8_t old[sizeof(ClaySettings)];
    if (size == (int)with_lat || size == (int)without_lat) {
      if (persist_read_data(SETTINGS_KEY, old, size) == size) {
        if (size == (int)with_lat) {
          memcpy(&s_settings, old, with_lat);
        } else {
          size_t prefix = offsetof(ClaySettings, LatitudeOffset);
          memcpy(&s_settings, old, prefix);
          memcpy((uint8_t *)&s_settings + prefix + sizeof(int),
                 old + prefix, without_lat - prefix);
        }
      }
    }
  }
  validate();
}
ClaySettings *settings_get(void) { return &s_settings; }
void settings_save(void) {
  SavedSettings saved = {
    .magic = SETTINGS_MAGIC, .version = SETTINGS_VERSION,
    .size = sizeof(ClaySettings), .value = s_settings
  };
  persist_write_data(SETTINGS_KEY, &saved, sizeof(saved));
}

static bool tuple_int(Tuple *t, int *out) {
  if (!t) return false;
  if (t->type == TUPLE_CSTRING) {
    char *end;
    long v = strtol(t->value->cstring, &end, 10);
    if (end == t->value->cstring || *end != '\0' || v < -100000 || v > 100000)
      return false;
    *out = (int)v;
    return true;
  }
  if (t->type == TUPLE_INT) {
    switch (t->length) {
      case 1: *out = (int)t->value->int8;  return true;
      case 2: *out = (int)t->value->int16; return true;
      case 4: *out = (int)t->value->int32; return true;
      default: return false;
    }
  }
  if (t->type == TUPLE_UINT) {
    switch (t->length) {
      case 1: *out = (int)t->value->uint8;  return true;
      case 2: *out = (int)t->value->uint16; return true;
      case 4: *out = (int)t->value->uint32; return true;
      default: return false;
    }
  }
  return false;
}


#define PARSE_INT(KEY, FIELD) do { \
  int value; \
  if (tuple_int(dict_find(iter, KEY), &value)) { \
    s_settings.FIELD = value; changed = true; \
  } \
} while (0)
#define PARSE_COLOR(KEY, FIELD) do { \
  Tuple *t = dict_find(iter, KEY); \
  if (t && (t->type == TUPLE_INT || t->type == TUPLE_UINT)) { \
    s_settings.FIELD = GColorFromHEX(t->value->uint32); changed = true; \
  } \
} while (0)
#define PARSE_STRING(KEY, FIELD) do { \
  Tuple *t = dict_find(iter, KEY); \
  if (t && t->type == TUPLE_CSTRING) { \
    strncpy(s_settings.FIELD, t->value->cstring, sizeof(s_settings.FIELD)-1); \
    s_settings.FIELD[sizeof(s_settings.FIELD)-1] = '\0'; changed = true; \
  } \
} while (0)
bool settings_update_from_dict(DictionaryIterator *iter) {
  bool changed = false;
  PARSE_COLOR(MESSAGE_KEY_BackgroundColor, BackgroundColor);
  PARSE_COLOR(MESSAGE_KEY_ForegroundColor, ForegroundColor);
  PARSE_COLOR(MESSAGE_KEY_DayLand, DayLand);
  PARSE_COLOR(MESSAGE_KEY_DayWater, DayWater);
  PARSE_COLOR(MESSAGE_KEY_DayIce, DayIce);
  PARSE_COLOR(MESSAGE_KEY_NightLand, NightLand);
  PARSE_COLOR(MESSAGE_KEY_NightWater, NightWater);
  PARSE_COLOR(MESSAGE_KEY_NightIce, NightIce);
  PARSE_INT(MESSAGE_KEY_DayMap, DayMapIndex);
  PARSE_INT(MESSAGE_KEY_NightMap, NightMapIndex);
  PARSE_INT(MESSAGE_KEY_LongitudeOffset, LongitudeOffset);
  PARSE_INT(MESSAGE_KEY_LatitudeOffset, LatitudeOffset);
  PARSE_INT(MESSAGE_KEY_CenterFocus, CenterFocus);
  Tuple *projection_t = dict_find(iter, MESSAGE_KEY_MapProjection);
  int projection_value;
  if (tuple_int(projection_t, &projection_value) &&
      (projection_value == 0 || projection_value == 1)) {
    s_settings.MapProjection = projection_value;
    changed = true;
  } else {
  }
  PARSE_STRING(MESSAGE_KEY_TimeFont, TimeFont);
  PARSE_STRING(MESSAGE_KEY_DateFont, DateFont);
  validate();
  return changed;
}


// PRE CUSTOM MAP STUFF
// #include <pebble.h>
// #include "settings.h"

// static ClaySettings s_settings;

// static void prv_default_settings() {
//   s_settings.BackgroundColor = GColorBlack;
//   s_settings.ForegroundColor = GColorWhite;
//   s_settings.DayMapIndex = 0;
//   s_settings.NightMapIndex = 0;
//   s_settings.LongitudeOffset = 0;
//   s_settings.CenterFocus = 0;
// }

// void settings_init(void) {
//   prv_default_settings();
//   persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
// }

// ClaySettings* settings_get(void) {
//   return &s_settings;
// }

// void settings_save(void) {
//   persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
// }

// bool settings_update_from_dict(DictionaryIterator *iter) {
//   bool changed = false;

//   Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_BackgroundColor);
//   if (bg_color_t) {
//     s_settings.BackgroundColor = GColorFromHEX(bg_color_t->value->int32);
//     changed = true;
//   }

//   Tuple *fg_color_t = dict_find(iter, MESSAGE_KEY_ForegroundColor);
//   if (fg_color_t) {
//     s_settings.ForegroundColor = GColorFromHEX(fg_color_t->value->int32);
//     changed = true;
//   }

//   Tuple *day_map_t = dict_find(iter, MESSAGE_KEY_DayMap);
//   if (day_map_t) {
//     s_settings.DayMapIndex = atoi(day_map_t->value->cstring);
//     changed = true;
//   }

//   Tuple *night_map_t = dict_find(iter, MESSAGE_KEY_NightMap);
//   if (night_map_t) {
//     s_settings.NightMapIndex = atoi(night_map_t->value->cstring);
//     changed = true;
//   }

//   Tuple *longitude_t = dict_find(iter, MESSAGE_KEY_LongitudeOffset);
//   if (longitude_t) {
//     s_settings.LongitudeOffset = longitude_t->value->int32;
//     changed = true;
//   }

//   Tuple *focus_t = dict_find(iter, MESSAGE_KEY_CenterFocus);
//   if (focus_t) {
//     s_settings.CenterFocus = atoi(focus_t->value->cstring);
//     changed = true;
//   }

//   return changed;
// }