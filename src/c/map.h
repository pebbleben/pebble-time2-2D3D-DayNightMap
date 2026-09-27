#pragma once
#include <pebble.h>

void map_init(Layer *parent_layer, GRect bounds);
void map_deinit(void);
void map_set_time_offset(int offset);
void map_force_redraw(void);
void map_reload_bitmaps(int day_index, int night_index);