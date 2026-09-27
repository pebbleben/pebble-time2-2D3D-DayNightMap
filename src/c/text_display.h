#pragma once
#include <pebble.h>
void text_display_init(Layer *parent_layer, GRect bounds);
void text_display_deinit(void);
void text_display_update_fonts(void);
void text_display_update_time(struct tm *tick_time);
void text_display_update_battery(BatteryChargeState charge_state);
void text_display_apply_colors(GColor fg_color);
