#include <pebble.h>
#include "text_display.h"
#include "layout.h"
#include "settings.h"

// ================================================================= //
#define SCREENSHOT_MODE 0 
// ================================================================= //

static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static Layer *s_battery_layer;
static BatteryChargeState s_battery_state;
static GColor s_fg_color;
static GFont s_oswald_time_font;      // <-- new: custom font handle
static GFont s_barlow_condensed_time_font;      // <-- new: custom font handle
static GFont s_barlow_time_font;      // <-- new: custom font handle


static GRect s_bounds;

// --- DYNAMIC FONT HELPERS ---
static GFont get_time_font(const char* font_name) {
  if (strcmp(font_name, "bitham") == 0) return fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  if (strcmp(font_name, "serif") == 0) return fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD);
  if (strcmp(font_name, "oswald") == 0 && s_oswald_time_font) return s_oswald_time_font;
  if (strcmp(font_name, "barlow_condensed") == 0 && s_barlow_condensed_time_font) return s_barlow_condensed_time_font;
  if (strcmp(font_name, "barlow") == 0 && s_barlow_time_font) return s_barlow_time_font;
  
  
  if (strcmp(font_name, "roboto") == 0) {
#if PBL_DISPLAY_HEIGHT >= 200
    return fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
#else
    // Roboto 49 is too big for the standard Time/Steel screen. 
    // Fall back to a beautiful, clean, modern alternative that actually fits!
    return fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT); 
#endif
  }
  
  // EXACT ORIGINAL DEFAULT
#ifdef PBL_COLOR
  #if PBL_DISPLAY_HEIGHT >= 200
    return fonts_get_system_font(FONT_KEY_LECO_60_NUMBERS_AM_PM);
  #elif PBL_DISPLAY_HEIGHT >= 180
    return fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS);
  #else
    return fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
  #endif
#else
  return fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS);
#endif
}

// Platform-specific pixel nudging to center everything perfectly
static int get_time_y_offset(const char* font_name) {
#if PBL_DISPLAY_HEIGHT >= 200
  // --- PEBBLE TIME 2 (EMERY) OFFSETS ---
  if (strcmp(font_name, "oswald") == 0) return 6;   // <-- new: lower Oswald  
  if (strcmp(font_name, "barlow_condensed") == 0) return 2;   // <-- new: lower Oswald  
  if (strcmp(font_name, "barlow") == 0) return 2;   // <-- new: lower Oswald  
  if (strcmp(font_name, "bitham") == 0) return 8;  // Lowered 4px
  if (strcmp(font_name, "roboto") == 0) return 8;  
  if (strcmp(font_name, "serif") == 0) return 23;  // Lowered 4px
  return 0; // Default Retro
#else
  // --- PEBBLE TIME STEEL / BASALT OFFSETS ---
//   if (strcmp(font_name, "oswald") == 0) return 5;    // <-- new: lower Oswald  
  if (strcmp(font_name, "bitham") == 0) return 2;  // Lifted 2px
  if (strcmp(font_name, "roboto") == 0) return 2;  // Offset for Bitham Light fallback
  if (strcmp(font_name, "serif") == 0) return 11; // Lifted 3px
  return 0; // Default Retro
#endif
}

// Every single style now has an auto-shrink array so they NEVER cut off on small screens!
static void apply_date_font(TextLayer *layer, const char* font_name, int max_w) {
  const char **candidates;
  size_t num_candidates = 0;

  static const char *bitham_cands[] = { FONT_KEY_BITHAM_30_BLACK, FONT_KEY_GOTHIC_28_BOLD, FONT_KEY_GOTHIC_24_BOLD };
  static const char *serif_cands[]  = { FONT_KEY_DROID_SERIF_28_BOLD, FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_18_BOLD };
  static const char *small_cands[]  = { FONT_KEY_GOTHIC_18_BOLD };
  static const char *clean_cands[]  = { FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_18_BOLD, FONT_KEY_GOTHIC_14_BOLD };

  if (strcmp(font_name, "bitham") == 0) {
    candidates = bitham_cands;
    num_candidates = ARRAY_LENGTH(bitham_cands);
  } else if (strcmp(font_name, "serif") == 0) {
    candidates = serif_cands;
    num_candidates = ARRAY_LENGTH(serif_cands);
  } else if (strcmp(font_name, "small") == 0) {
    candidates = small_cands;
    num_candidates = ARRAY_LENGTH(small_cands);
  } else {
    candidates = clean_cands;
    num_candidates = ARRAY_LENGTH(clean_cands);
  }

  // Measure text and pick the absolute largest one that physically fits on the screen
  const char *chosen = candidates[num_candidates - 1];
  for (size_t i = 0; i < num_candidates; i++) {
    text_layer_set_font(layer, fonts_get_system_font(candidates[i]));
    if (text_layer_get_content_size(layer).w <= max_w) {
      chosen = candidates[i];
      break;
    }
  }
  text_layer_set_font(layer, fonts_get_system_font(chosen));
}

// --- BATTERY DRAWING LOGIC ---
static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_stroke_color(ctx, s_fg_color);
  graphics_context_set_fill_color(ctx, s_fg_color);

  int bat_w = 20;
  int bat_h = 10;
  int bat_x = bounds.size.w - bat_w - 4; 
  int bat_y = 4;

  graphics_draw_rect(ctx, GRect(bat_x, bat_y, bat_w, bat_h));
  graphics_fill_rect(ctx, GRect(bat_x + bat_w, bat_y + 3, 2, 4), 0, GCornerNone);

  int max_inner_w = bat_w - 4;
  int charge_w = (s_battery_state.charge_percent * max_inner_w) / 100;
  
  if (charge_w > 0) {
    graphics_fill_rect(ctx, GRect(bat_x + 2, bat_y + 2, charge_w, bat_h - 4), 0, GCornerNone);
  }

  if (s_battery_state.is_charging) {
    graphics_draw_line(ctx, GPoint(bat_x - 6, bat_y + 4), GPoint(bat_x - 6, bat_y + 6));
    graphics_draw_line(ctx, GPoint(bat_x - 7, bat_y + 5), GPoint(bat_x - 5, bat_y + 5));
  }
}

// Applies fonts & positions dynamically using 100% ORIGINAL math
void text_display_update_fonts(void) {
  ClaySettings *settings = settings_get();
  int W = s_bounds.size.w;
  int H = s_bounds.size.h;
  
  // --- 1. SET TIME FONT (Fixed bounds to prevent shifting) ---
  text_layer_set_font(s_time_layer, get_time_font(settings->TimeFont));
  
  int base_time_y = (H * 2) / LAYOUT_H; 
  int time_y = base_time_y + get_time_y_offset(settings->TimeFont);
  
  int time_h = (H * 65) / LAYOUT_H; 
  layer_set_frame(text_layer_get_layer(s_time_layer), GRect(0, time_y, W, time_h));


  // --- 2. SET DATE FONT (Original W * 2 Dummy trick) ---
  const char* current_text = text_layer_get_text(s_date_layer);
  layer_set_frame(text_layer_get_layer(s_date_layer), GRect(0, 0, W * 2, (H * 40) / LAYOUT_H));
  
  // Ensure dummy text is set so the auto-scaling algorithm measures properly
  text_layer_set_text(s_date_layer, "Wed, Sep 30"); 
  apply_date_font(s_date_layer, settings->DateFont, W);
  
  // Original Slack Centering Math
  int map_bottom = (H * (MAP_TOP_168 + MAP_H_168)) / LAYOUT_H;
  int band = H - map_bottom;
  int line_h = text_layer_get_content_size(s_date_layer).h + 4;
  
#if defined(PBL_ROUND)
  int date_y = map_bottom + 2;
#else
  int slack = band - line_h;
  int date_y = map_bottom + (slack > 12 ? slack / 2 : 2); 
#endif

  if (date_y < map_bottom) date_y = map_bottom;
  
  // Set the final proper frame and restore the real text
  layer_set_frame(text_layer_get_layer(s_date_layer), GRect(0, date_y, W, H - date_y));
  text_layer_set_text(s_date_layer, current_text);
}

void text_display_init(Layer *parent_layer, GRect bounds) {
  s_bounds = bounds;
  int W = bounds.size.w;
  int H = bounds.size.h;
  s_fg_color = GColorWhite;

  s_time_layer = text_layer_create(GRect(0, 0, W, 80));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(parent_layer, text_layer_get_layer(s_time_layer));

  s_date_layer = text_layer_create(GRect(0, 0, W, 40));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_date_layer, GTextOverflowModeFill);
  text_layer_set_text(s_date_layer, "Wed, Sep 30"); 
  layer_add_child(parent_layer, text_layer_get_layer(s_date_layer));

  s_battery_layer = layer_create(GRect(W - 40, 0, 40, (H * 40) / LAYOUT_H));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(parent_layer, s_battery_layer);
  
#ifdef RESOURCE_ID_FONT_OSWALD_BOLD_54
  s_oswald_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_OSWALD_BOLD_54));
#endif
#ifdef RESOURCE_ID_FONT_Barlow_SemiCondensed_Bold_62
  s_barlow_condensed_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_Barlow_SemiCondensed_Bold_62));
#endif
#ifdef RESOURCE_ID_FONT_Barlow_Semi_Bold_62
  s_barlow_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_Barlow_Semi_Bold_62));
#endif

  text_display_update_fonts();
}

void text_display_deinit(void) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  layer_destroy(s_battery_layer);
  if (s_oswald_time_font) {
    fonts_unload_custom_font(s_oswald_time_font);
    s_oswald_time_font = NULL;
  }  
  if (s_barlow_condensed_time_font) {
    fonts_unload_custom_font(s_barlow_condensed_time_font);
    s_barlow_condensed_time_font = NULL;
  }  
  if (s_barlow_time_font) {
    fonts_unload_custom_font(s_barlow_time_font);
    s_barlow_time_font = NULL;
  }      
}

void text_display_update_time(struct tm *tick_time) {
#if SCREENSHOT_MODE
  // Force clean text for screenshots
  text_layer_set_text(s_time_layer, "10:09");
  text_layer_set_text(s_date_layer, "Wed, Sep 30");
#else
  static char time_text[] = "00:00xx";
  static char date_text[16];

  strftime(date_text, sizeof(date_text), "%a, %b %e", tick_time);
  text_layer_set_text(s_date_layer, date_text);

  strftime(time_text, sizeof(time_text),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  if (!clock_is_24h_style() && time_text[0] == '0') {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }
  text_layer_set_text(s_time_layer, time_text);
#endif
}

void text_display_update_battery(BatteryChargeState charge_state) {
#if SCREENSHOT_MODE
  s_battery_state.charge_percent = 100;
  s_battery_state.is_charging = false;
  s_battery_state.is_plugged = false;
#else
  s_battery_state = charge_state;
#endif

  if (s_battery_layer) layer_mark_dirty(s_battery_layer);
}

void text_display_apply_colors(GColor fg_color) {
  s_fg_color = fg_color;
  text_layer_set_text_color(s_time_layer, fg_color);
  text_layer_set_text_color(s_date_layer, fg_color);
  if (s_battery_layer) layer_mark_dirty(s_battery_layer);
}

