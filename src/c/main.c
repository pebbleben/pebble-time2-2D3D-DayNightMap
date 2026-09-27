#include <pebble.h>
#include "settings.h"
#include "text_display.h"
#include "map.h"

#define TIME_OFFSET_PERSIST 1
#define REDRAW_INTERVAL 15

static Window *s_window;
static int s_redraw_counter;

static void update_display_from_settings() {
  ClaySettings *settings = settings_get();
  
  window_set_background_color(s_window, settings->BackgroundColor);
  text_display_apply_colors(settings->ForegroundColor);
  text_display_update_fonts(); 
  map_reload_bitmaps(settings->DayMapIndex, settings->NightMapIndex);
  map_force_redraw();
}

static void app_message_inbox_received(DictionaryIterator *iterator, void *context) {
  // Check for time offset from phone (Key 0)
  Tuple *offset_t = dict_find(iterator, 0); 
  if (offset_t && (offset_t->type == TUPLE_INT || offset_t->type == TUPLE_UINT)) {
    int time_offset = offset_t->value->int32 - (int)time(NULL);
    persist_write_int(TIME_OFFSET_PERSIST, time_offset);
    map_set_time_offset(time_offset);
    map_force_redraw();
  }

  // Handle Clay settings dictionary
  if (settings_update_from_dict(iterator)) {
    settings_save();
    update_display_from_settings();
  }
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  text_display_update_battery(battery_state_service_peek());
  text_display_update_time(tick_time);

  s_redraw_counter++;
  if (s_redraw_counter >= REDRAW_INTERVAL) {
    map_force_redraw();
    s_redraw_counter = 0;
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Initialize Text and Map modules
  map_init(window_layer, bounds);
  text_display_init(window_layer, bounds);

  // Apply colors and load maps initially
  update_display_from_settings();

  // Force first tick
  time_t now = time(NULL);
  handle_minute_tick(localtime(&now), MINUTE_UNIT);
}

static void prv_window_unload(Window *window) {
  text_display_deinit();
  map_deinit();
}

static void prv_init(void) {
  settings_init();
  s_redraw_counter = REDRAW_INTERVAL; // Force draw on start

  if (persist_exists(TIME_OFFSET_PERSIST)) {
    map_set_time_offset(persist_read_int(TIME_OFFSET_PERSIST));
  }

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  battery_state_service_subscribe(text_display_update_battery);

  app_message_register_inbox_received(app_message_inbox_received);
  app_message_open(1024, 128);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}



// #include <pebble.h>
// #include "main.h"

// #define STR_SIZE 20
// #define TIME_OFFSET_PERSIST 1
// #define REDRAW_INTERVAL 15

// // The layout is designed against a 144x168 screen and scaled from the real
// // root-layer bounds at runtime, so it works on:
// //   144x168 -> aplite, basalt, diorite, flint
// //   180x180 -> chalk
// //   200x228 -> emery (Pebble Time 2)
// #define LAYOUT_H    168
// #if PBL_DISPLAY_HEIGHT >= 200
//   #define MAP_TOP_168 58   // emery / Time 2 -- globe pushed down toward the date
// #else
//   #define MAP_TOP_168 50   // 144x168 and 180x180
// #endif
// #define MAP_H_168   72

// #define BATTERY_FONT FONT_KEY_GOTHIC_18

// // ---- terminator highlight --------------------------------------------------
// #define TERMINATOR_LINE 1
// #define TERMINATOR_PX   GColorLightGrayARGB8
// #define TERM_MAX_W      256

// static Window *window;
// static TextLayer *time_text_layer;
// static TextLayer *date_text_layer;
// static TextLayer *s_battery_layer;

// static GBitmap *world_bitmap;
// #ifdef PBL_COLOR
// static GBitmap *night_bitmap;
// static uint8_t s_term_flags[2][TERM_MAX_W];
// #else
// static GBitmap *image;
// #endif
// static Layer *canvas;

// static int redraw_counter;
// char *s;
// int time_offset;

// // A struct for our specific settings (see main.h)
// ClaySettings settings;

// // ---- fonts -----------------------------------------------------------------

// static const char *time_font_key(void) {
// #ifdef PBL_COLOR
//   #if PBL_DISPLAY_HEIGHT >= 200
//     return FONT_KEY_LECO_60_NUMBERS_AM_PM;
//   #elif PBL_DISPLAY_HEIGHT >= 180
//     return FONT_KEY_LECO_38_BOLD_NUMBERS;
//   #else
//     return FONT_KEY_LECO_36_BOLD_NUMBERS;
//   #endif
// #else
//   return FONT_KEY_BITHAM_42_MEDIUM_NUMBERS;
// #endif
// }

// static const char *pick_date_font(TextLayer *layer, int max_w) {
//   static const char *candidates[] = {
// #if PBL_DISPLAY_HEIGHT >= 200
//     FONT_KEY_BITHAM_30_BLACK,
// #endif
//     FONT_KEY_GOTHIC_28_BOLD,
//     FONT_KEY_GOTHIC_24_BOLD,
//     FONT_KEY_GOTHIC_18_BOLD,
//   };

//   const char *chosen = candidates[ARRAY_LENGTH(candidates) - 1];
//   for (size_t i = 0; i < ARRAY_LENGTH(candidates); i++) {
//     text_layer_set_font(layer, fonts_get_system_font(candidates[i]));
//     if (text_layer_get_content_size(layer).w <= max_w) {
//       chosen = candidates[i];
//       break;
//     }
//   }
//   text_layer_set_font(layer, fonts_get_system_font(chosen));
//   return chosen;
// }

// // ---- layout ----------------------------------------------------------------

// static GRect map_rect(GRect bounds) {
//   return GRect(0,
//                (bounds.size.h * MAP_TOP_168) / LAYOUT_H,
//                bounds.size.w,
//                (bounds.size.h * MAP_H_168) / LAYOUT_H);
// }

// static int map_bottom_y(GRect bounds) {
//   return (bounds.size.h * (MAP_TOP_168 + MAP_H_168)) / LAYOUT_H;
// }

// // ---- battery ---------------------------------------------------------------

// static void handle_battery(BatteryChargeState charge_state) {
//   static char battery_text[8] = "100+0";

//   if (charge_state.is_charging) {
//     snprintf(battery_text, sizeof(battery_text), "%d+", charge_state.charge_percent);
//   } else {
//     snprintf(battery_text, sizeof(battery_text), "%d", charge_state.charge_percent);
//   }
//   text_layer_set_text(s_battery_layer, battery_text);
// }

// // ---- sun position ----------------------------------------------------------

// static void sun_position(int now, int *sun_x, int *sun_y) {
//   int leap_years = (int)((float)now / 131487192.0);
//   float day_of_year = now - (((int)((float)now / 31556926.0) * 365 + leap_years) * 86400);
//   day_of_year = day_of_year / 86400.0;
//   float time_of_day = day_of_year - (int)day_of_year;
//   day_of_year = day_of_year / 365.0;

//   *sun_x = (int)((float)TRIG_MAX_ANGLE * (1.0 - time_of_day));
//   *sun_y = (int)(-sin_lookup((day_of_year - 0.2164) * TRIG_MAX_ANGLE) * .26 * .25);
// }

// static bool is_night(int dx, int dy, GRect map, int sun_x, int sun_y) {
//   int x_angle = (int)((float)TRIG_MAX_ANGLE * (float)dx / (float)map.size.w);
//   int y_angle = (int)((float)TRIG_MAX_ANGLE * (float)dy / (float)(map.size.h * 2)) - TRIG_MAX_ANGLE / 4;
//   float angle = ((float)sin_lookup(sun_y) / (float)TRIG_MAX_RATIO) * ((float)sin_lookup(y_angle) / (float)TRIG_MAX_RATIO);
//   angle = angle + ((float)cos_lookup(sun_y) / (float)TRIG_MAX_RATIO) * ((float)cos_lookup(y_angle) / (float)TRIG_MAX_RATIO) * ((float)cos_lookup(sun_x - x_angle) / (float)TRIG_MAX_RATIO);
//   return angle < 0;
// }

// #ifdef PBL_COLOR

// static uint8_t read_px(GBitmap *bmp, const uint8_t *row, int x) {
//   switch (gbitmap_get_format(bmp)) {
//     case GBitmapFormat8Bit:
//     case GBitmapFormat8BitCircular:
//       return row[x];

//     case GBitmapFormat1Bit:
//       return (row[x >> 3] & (1 << (x & 7))) ? 0xFF : 0x00;

//     case GBitmapFormat1BitPalette:
//       return (uint8_t)gbitmap_get_palette(bmp)[(row[x >> 3] >> (x & 7)) & 0x01].argb;

//     case GBitmapFormat2BitPalette:
//       return (uint8_t)gbitmap_get_palette(bmp)[(row[x >> 2] >> (2 * (x & 3))) & 0x03].argb;

//     case GBitmapFormat4BitPalette:
//       return (uint8_t)gbitmap_get_palette(bmp)[(row[x >> 1] >> (4 * (x & 1))) & 0x0F].argb;

//     default:
//       return 0xC0;
//   }
// }

// static void log_bitmap(const char *tag, GBitmap *b) {
//   GSize s2 = gbitmap_get_bounds(b).size;
//   APP_LOG(APP_LOG_LEVEL_DEBUG, "%s %dx%d fmt=%d row=%d  (want fmt=1)",
//           tag, s2.w, s2.h, (int)gbitmap_get_format(b), gbitmap_get_bytes_per_row(b));
// }

// typedef struct { int x, y, w, h; } SrcRect;

// static SrcRect globe_src(GBitmap *bmp) {
//   GSize s2 = gbitmap_get_bounds(bmp).size;
//   SrcRect r = { 0, 0, s2.w, s2.h };
//   return r;
// }

// #endif // PBL_COLOR

// // ---- monochrome: rebuild the offscreen 1-bit globe -------------------------

// static void draw_earth(void) {
// #ifndef PBL_COLOR
//   int sun_x, sun_y;
//   sun_position((int)time(NULL) + time_offset, &sun_x, &sun_y);

//   GSize size        = gbitmap_get_bounds(world_bitmap).size;
//   const uint8_t *world_data = gbitmap_get_data(world_bitmap);
//   int world_stride  = gbitmap_get_bytes_per_row(world_bitmap);
//   uint8_t *img_data = gbitmap_get_data(image);
//   int img_stride    = gbitmap_get_bytes_per_row(image);
//   GRect map         = GRect(0, 0, size.w, size.h);

//   for (int y = 0; y < size.h; y++) {
//     for (int x = 0; x < size.w; x++) {
//       int wbyte = y * world_stride + (x / 8);
//       int ibyte = y * img_stride   + (x / 8);
//       if (is_night(x, y, map, sun_x, sun_y) ^ (0x1 & (world_data[wbyte] >> (x % 8)))) {
//         img_data[ibyte] |=  (0x1 << (x % 8));
//       } else {
//         img_data[ibyte] &= ~(0x1 << (x % 8));
//       }
//     }
//   }
// #endif
//   layer_mark_dirty(canvas);
// }

// // ---- the layer update proc -------------------------------------------------

// static void draw_watch(struct Layer *layer, GContext *ctx) {
//   GRect bounds = layer_get_bounds(layer);
//   GRect map    = map_rect(bounds);

// #ifdef PBL_COLOR
//   int sun_x, sun_y;
//   sun_position((int)time(NULL) + time_offset, &sun_x, &sun_y);

//   GBitmap *fb = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
//   if (!fb) {
//     return;
//   }

//   uint8_t *fb_data = gbitmap_get_data(fb);
//   int fb_stride    = gbitmap_get_bytes_per_row(fb);

//   GSize dsz            = gbitmap_get_bounds(world_bitmap).size;
//   const uint8_t *ddata = gbitmap_get_data(world_bitmap);
//   int dstride          = gbitmap_get_bytes_per_row(world_bitmap);

//   GSize nsz            = gbitmap_get_bounds(night_bitmap).size;
//   const uint8_t *ndata = gbitmap_get_data(night_bitmap);
//   int nstride          = gbitmap_get_bytes_per_row(night_bitmap);

//   SrcRect dsrc = globe_src(world_bitmap);
//   SrcRect nsrc = globe_src(night_bitmap);

//   int d_drawn_h = (dsrc.h * map.size.w) / dsrc.w;
//   int n_drawn_h = (nsrc.h * map.size.w) / nsrc.w;
//   if (d_drawn_h < 1) d_drawn_h = 1;
//   if (n_drawn_h < 1) n_drawn_h = 1;

//   int cols = map.size.w;
//   if (cols > TERM_MAX_W) cols = TERM_MAX_W;

//   static bool logged = false;
//   if (!logged) {
//     log_bitmap("day", world_bitmap);
//     log_bitmap("night", night_bitmap);
//     APP_LOG(APP_LOG_LEVEL_DEBUG, "map %dx%d fb_stride %d drawn_h %d",
//             map.size.w, map.size.h, fb_stride, d_drawn_h);
//     logged = true;
//   }

//   for (int dy = 0; dy < map.size.h; dy++) {
//     int d_sy = dsrc.y + (dy * dsrc.h) / d_drawn_h;
//     int n_sy = nsrc.y + (dy * nsrc.h) / n_drawn_h;
//     if (d_sy >= dsrc.y + dsrc.h) d_sy = dsrc.y + dsrc.h - 1;
//     if (n_sy >= nsrc.y + nsrc.h) n_sy = nsrc.y + nsrc.h - 1;

//     const uint8_t *drow = ddata + d_sy * dstride;
//     const uint8_t *nrow = ndata + n_sy * nstride;
//     uint8_t *fb_row     = fb_data + (map.origin.y + dy) * fb_stride + map.origin.x;

//     uint8_t *cur_flags  = s_term_flags[dy & 1];
//     uint8_t *prev_flags = s_term_flags[(dy + 1) & 1];
//     int prev_night = -1;

//     for (int dx = 0; dx < cols; dx++) {
//       int night = is_night(dx, dy, map, sun_x, sun_y) ? 1 : 0;

//       bool on_seam = false;
// #if TERMINATOR_LINE
//       if (prev_night >= 0 && night != prev_night) on_seam = true;
//       if (dy > 0      && prev_flags[dx] != night) on_seam = true;
// #endif

//       uint8_t px;
//       if (on_seam) {
//         px = TERMINATOR_PX;
//       } else {
//         px = night ? read_px(night_bitmap, nrow, (dx * nsz.w) / map.size.w)
//                    : read_px(world_bitmap, drow, (dx * dsz.w) / map.size.w);
//         if ((px & 0xC0) == 0) px = 0xC0;
//       }
//       fb_row[dx] = px | 0xC0;

//       cur_flags[dx] = (uint8_t)night;
//       prev_night    = night;
//     }
//   }
//   graphics_release_frame_buffer(ctx, fb);
// #else
//   GSize size = gbitmap_get_bounds(image).size;
//   graphics_draw_bitmap_in_rect(ctx, image, GRect(map.origin.x, map.origin.y, size.w, size.h));
// #endif
// }

// // ---- tick ------------------------------------------------------------------

// static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
//   handle_battery(battery_state_service_peek());

//   static char time_text[] = "00:00xx";
//   static char date_text[16];

//   strftime(date_text, sizeof(date_text), "%a, %b %e", tick_time);
//   text_layer_set_text(date_text_layer, date_text);

//   strftime(time_text, sizeof(time_text), "%I:%M", tick_time);
//   if (time_text[0] == '0') {
//     memmove(time_text, &time_text[1], sizeof(time_text) - 1);
//   }
//   text_layer_set_text(time_text_layer, time_text);

//   redraw_counter++;
//   if (redraw_counter >= REDRAW_INTERVAL) {
//     draw_earth();
//     redraw_counter = 0;
//   }
// }

// // ---- Clay settings ---------------------------------------------------------

// // Initialize the default settings
// static void prv_default_settings() {
//   settings.BackgroundColor = GColorBlack;
//   settings.ForegroundColor = GColorWhite;
//   settings.SecondTick = false;
//   settings.Animations = false;
//   settings.DayMapIndex = 0;
//   settings.NightMapIndex = 0;
// }

// // Read settings from persistent storage
// static void prv_load_settings() {
//   prv_default_settings();
//   persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
// }

// // Load day bitmap based on settings
// static void load_day_bitmap() {
//   if (world_bitmap) {
//     gbitmap_destroy(world_bitmap);
//   }
  
// #ifdef PBL_COLOR
//   switch (settings.DayMapIndex) {
//     case 0:
//       world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DAY_01_CHARLIE);
//       break;
//     case 1:
//       world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DAY_02_BLUE_MARBLE);
//       break;
//     default:
//       world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DAY_01_CHARLIE);
//       break;
//   }
// #else
//   world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_WORLD);
// #endif
// }

// // Load night bitmap based on settings
// static void load_night_bitmap() {
// #ifdef PBL_COLOR
//   if (night_bitmap) {
//     gbitmap_destroy(night_bitmap);
//   }
  
//   switch (settings.NightMapIndex) {
//     case 0:
//       night_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NIGHT_01_DITHER);
//       break;
//     case 1:
//       night_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NIGHT_02_CLEAN);
//       break;
//     default:
//       night_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NIGHT_01_DITHER);
//       break;
//   }
// #endif
// }

// // Update the display elements
// static void prv_update_display() {
//   // Background color
//   if (settings.BackgroundColor.argb != GColorBlackARGB8 || 
//       settings.ForegroundColor.argb != GColorWhiteARGB8) {
//     window_set_background_color(window, settings.BackgroundColor);
//   } else {
// #ifdef BLACK_ON_WHITE
//     window_set_background_color(window, GColorWhite);
// #else
//     window_set_background_color(window, GColorBlack);
// #endif
//   }

//   // Update text colors
//   text_layer_set_text_color(time_text_layer, settings.ForegroundColor);
//   text_layer_set_text_color(date_text_layer, settings.ForegroundColor);
//   text_layer_set_text_color(s_battery_layer, settings.ForegroundColor);

//   // Reload bitmaps based on new settings
//   load_day_bitmap();
//   load_night_bitmap();
  
//   // Redraw the earth and clear display caches
//   draw_earth();
// }

// // Save the settings to persistent storage
// static void prv_save_settings() {
//   persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
//   prv_update_display();
// }

// // ---- Combined Message Inbox Handler -----------------------------------------
// static void app_message_inbox_received(DictionaryIterator *iterator, void *context) {
//   bool settings_changed = false;

//   // 1. Check for time offset from phone (Assuming Key 0 from previous code)
//   Tuple *offset_t = dict_find(iterator, 0); 
//   if (offset_t) {
//     int unixtime = offset_t->value->int32;
//     int now = (int)time(NULL);
//     time_offset = unixtime - now;
//     status_t s = persist_write_int(TIME_OFFSET_PERSIST, time_offset);
//     if (s) {
//       APP_LOG(APP_LOG_LEVEL_DEBUG, "Saved time offset %d", time_offset);
//     }
//     draw_earth();
//   }

//   // 2. Check for Clay Settings (Background Color)
//   Tuple *bg_color_t = dict_find(iterator, MESSAGE_KEY_BackgroundColor);
//   if (bg_color_t) {
//     settings.BackgroundColor = GColorFromHEX(bg_color_t->value->int32);
//     settings_changed = true;
//   }

//   // Foreground Color
//   Tuple *fg_color_t = dict_find(iterator, MESSAGE_KEY_ForegroundColor);
//   if (fg_color_t) {
//     settings.ForegroundColor = GColorFromHEX(fg_color_t->value->int32);
//     settings_changed = true;
//   }

//   // Second Tick
//   Tuple *second_tick_t = dict_find(iterator, MESSAGE_KEY_SecondTick);
//   if (second_tick_t) {
//     settings.SecondTick = second_tick_t->value->int32 == 1;
//     settings_changed = true;
//   }

//   // Animations
//   Tuple *animations_t = dict_find(iterator, MESSAGE_KEY_Animations);
//   if (animations_t) {
//     settings.Animations = animations_t->value->int32 == 1;
//     settings_changed = true;
//   }

//   // Day Map
//   Tuple *day_map_t = dict_find(iterator, MESSAGE_KEY_DayMap);
//   if (day_map_t) {
//     settings.DayMapIndex = atoi(day_map_t->value->cstring);
//     settings_changed = true;
//   }

//   // Night Map
//   Tuple *night_map_t = dict_find(iterator, MESSAGE_KEY_NightMap);
//   if (night_map_t) {
//     settings.NightMapIndex = atoi(night_map_t->value->cstring);
//     settings_changed = true;
//   }

//   // Save the new settings to persistent storage and refresh display
//   if (settings_changed) {
//     prv_save_settings();
//   }
// }

// // ---- window ----------------------------------------------------------------

// static void prv_window_load(Window *window) {
//   window_set_background_color(window, settings.BackgroundColor);

//   Layer *window_layer = window_get_root_layer(window);
//   GRect bounds = layer_get_bounds(window_layer);
//   int W = bounds.size.w;
//   int H = bounds.size.h;

//   //Local time
//   time_text_layer = text_layer_create(GRect(0, (H * 2) / LAYOUT_H, W, (H * 58) / LAYOUT_H));
//   text_layer_set_background_color(time_text_layer, GColorClear);
//   text_layer_set_text_color(time_text_layer, settings.ForegroundColor);
//   text_layer_set_font(time_text_layer, fonts_get_system_font(time_font_key()));
//   text_layer_set_text(time_text_layer, "");
//   text_layer_set_text_alignment(time_text_layer, GTextAlignmentCenter);
//   layer_add_child(window_layer, text_layer_get_layer(time_text_layer));

//   //Date
//   date_text_layer = text_layer_create(GRect(0, 0, W * 2, (H * 40) / LAYOUT_H));
//   text_layer_set_background_color(date_text_layer, GColorClear);
//   text_layer_set_text_color(date_text_layer, settings.ForegroundColor);
//   text_layer_set_text_alignment(date_text_layer, GTextAlignmentCenter);
//   text_layer_set_overflow_mode(date_text_layer, GTextOverflowModeFill);
//   text_layer_set_text(date_text_layer, "Wed, Sep 30");
//   pick_date_font(date_text_layer, W);
//   layer_add_child(window_layer, text_layer_get_layer(date_text_layer));

//   int map_bottom = map_bottom_y(bounds);
//   int band       = H - map_bottom;
//   GSize dsize    = text_layer_get_content_size(date_text_layer);
//   int line_h     = dsize.h + 4;

// #if defined(PBL_ROUND)
//   int date_y = map_bottom + 2;
// #else
//   int slack  = band - line_h;
//   int date_y = map_bottom + (slack > 12 ? slack / 2 : 2);
// #endif
//   if (date_y < map_bottom) date_y = map_bottom;
//   int date_h = H - date_y;
//   layer_set_frame(text_layer_get_layer(date_text_layer), GRect(0, date_y, W, date_h));

//   //BATTERY TEXT
//   s_battery_layer = text_layer_create(GRect(W - 34, 0, 32, (H * 40) / LAYOUT_H));
//   text_layer_set_text_color(s_battery_layer, settings.ForegroundColor);
//   text_layer_set_background_color(s_battery_layer, GColorClear);
//   text_layer_set_font(s_battery_layer, fonts_get_system_font(BATTERY_FONT));
//   text_layer_set_text_alignment(s_battery_layer, GTextAlignmentRight);
//   text_layer_set_text(s_battery_layer, "  -");
//   layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));

//   //BOTH COLOUR AND MONO
//   canvas = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
//   layer_set_update_proc(canvas, draw_watch);
//   layer_add_child(window_layer, canvas);

// #ifndef PBL_COLOR
//   image = gbitmap_create_blank(gbitmap_get_bounds(world_bitmap).size, GBitmapFormat1Bit);
// #endif

//   time_t now = time(NULL);
//   struct tm *now_tm = localtime(&now);
//   if (now_tm) {
//     handle_minute_tick(now_tm, MINUTE_UNIT);
//   }
//   draw_earth();
// }

// static void prv_window_unload(Window *window) {
//   text_layer_destroy(time_text_layer);
//   text_layer_destroy(date_text_layer);
//   text_layer_destroy(s_battery_layer);
//   battery_state_service_unsubscribe();
//   tick_timer_service_unsubscribe();
//   layer_destroy(canvas);
// #ifndef PBL_COLOR
//   gbitmap_destroy(image);
// #endif
// }

// static void prv_init(void) {
//   redraw_counter = 20;

//   // Load settings first
//   prv_load_settings();

//   time_offset = 0;
//   if (persist_exists(TIME_OFFSET_PERSIST)) {
//     time_offset = persist_read_int(TIME_OFFSET_PERSIST);
//     APP_LOG(APP_LOG_LEVEL_DEBUG, "loaded offset %d", time_offset);
//   }

//   // Load bitmaps based on settings
//   load_day_bitmap();
//   load_night_bitmap();

//   window = window_create();
//   window_set_window_handlers(window, (WindowHandlers) {
//     .load = prv_window_load,
//     .unload = prv_window_unload,
//   });

//   const bool animated = true;
//   window_stack_push(window, animated);

//   s = malloc(STR_SIZE);
//   tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
//   battery_state_service_subscribe(handle_battery);

//   // ONLY USE ONE INBOX RECEIVED HANDLER
//   app_message_register_inbox_received(app_message_inbox_received);
//   app_message_open(256, 256); // Expanded size just in case you send lots of config data
// }

// static void prv_deinit(void) {
//   free(s);
//   window_destroy(window);
//   gbitmap_destroy(world_bitmap);
// #ifdef PBL_COLOR
//   gbitmap_destroy(night_bitmap);
// #endif
// }

// int main(void) {
//   prv_init();
//   APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
//   app_event_loop();
//   prv_deinit();
// }