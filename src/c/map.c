#include <pebble.h>
#include "map.h"
#include "layout.h"
#include "settings.h"
#include "earth_math.h"

/* 0: normal, 1: plain light/shadow, 2: day texture only. */
#define MAP_DEBUG_MODE 0
#define TERMINATOR_LINE 1
#define MAX_MAP_WIDTH 512

static Layer *s_canvas;
static GBitmap *s_world_bitmap;
#ifdef PBL_COLOR
static GBitmap *s_night_bitmap;
#endif
static int s_day_index = -1, s_night_index = -1;
static int s_time_offset;
static uint8_t s_previous_row[MAX_MAP_WIDTH];

/* Full rectangular resources are expected (not sub-bitmaps). */
static uint8_t read_pixel(GBitmap *bmp, int x, int y) {
  const uint8_t *row = gbitmap_get_data(bmp)
      + y * gbitmap_get_bytes_per_row(bmp);
  switch (gbitmap_get_format(bmp)) {
    case GBitmapFormat1Bit:
      return (row[x >> 3] & (1u << (x & 7))) ? 0xFF : 0xC0;
#ifdef PBL_COLOR
    case GBitmapFormat8Bit: return row[x];
    case GBitmapFormat1BitPalette:
      return gbitmap_get_palette(bmp)[(row[x >> 3] >> (7-(x&7))) & 1].argb;
    case GBitmapFormat2BitPalette:
      return gbitmap_get_palette(bmp)[(row[x >> 2] >> (6-2*(x&3))) & 3].argb;
    case GBitmapFormat4BitPalette:
      return gbitmap_get_palette(bmp)[(row[x >> 1] >> (4*(1-(x&1)))) & 15].argb;
#endif
    default: return 0xC0;
  }
}
/* Temporary diagnostic build. Remove after identifying the mismatch. */
static void report_bitmap(const char *name, GBitmap *bmp) {
  if (!bmp) { APP_LOG(APP_LOG_LEVEL_INFO, "EARTH %s missing", name); return; }
  GRect b = gbitmap_get_bounds(bmp);
  APP_LOG(APP_LOG_LEVEL_INFO, "EARTH %s fmt=%d size=%dx%d origin=%d,%d stride=%d", name,
          (int)gbitmap_get_format(bmp), b.size.w, b.size.h,
          b.origin.x, b.origin.y, (int)gbitmap_get_bytes_per_row(bmp));
  uint32_t hash = 2166136261u;
  unsigned int land=0, ice=0, water=0, other=0;
  for (int y=0; y<b.size.h; ++y) for (int x=0; x<b.size.w; ++x) {
    uint8_t p = read_pixel(bmp,x,y) | 0xC0;
    hash = (hash ^ p) * 16777619u;
    if(p==0xCC) ++land; else if(p==0xCF) ++ice;
    else if(p==0xC3) ++water; else ++other;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "EARTH %s hash=%lu land=%u ice=%u water=%u other=%u",name,
          (unsigned long)hash,land,ice,water,other);
}
static void report_geometry(void) {
  EarthCamera c=earth_camera(0,0);
  EarthVector p;
  float u,v;
  earth_surface(c,0.6f,0.0f,&p); earth_uv(p,&u,&v);
  APP_LOG(APP_LOG_LEVEL_INFO,"EARTH TEST A xyz10000=%d,%d,%d uv10000=%d,%d",
      (int)(p.x*10000),(int)(p.y*10000),(int)(p.z*10000),(int)(u*10000),(int)(v*10000));
  earth_surface(c,0.0f,0.6f,&p); earth_uv(p,&u,&v);
  APP_LOG(APP_LOG_LEVEL_INFO,"EARTH TEST B xyz10000=%d,%d,%d uv10000=%d,%d",
      (int)(p.x*10000),(int)(p.y*10000),(int)(p.z*10000),(int)(u*10000),(int)(v*10000));
}
static uint8_t sample(GBitmap *bmp, float u, float v) {
  GSize size = gbitmap_get_bounds(bmp).size;
  int x = (int)(u * size.w);
  int y = (int)(v * size.h);
  if (x < 0) x = 0;
  if (x >= size.w) x = size.w-1;
  if (y < 0) y = 0;
  if (y >= size.h) y = size.h-1;
  return read_pixel(bmp, x, y);
}
static GColor pixel_color(ClaySettings *s, float u, float v, bool night) {
#if MAP_DEBUG_MODE == 1
  return night ? GColorBlack : GColorWhite;
#else
#if MAP_DEBUG_MODE == 2
  night = false;
#endif
#ifdef PBL_COLOR
  GBitmap *bmp = night ? s_night_bitmap : s_world_bitmap;
  uint8_t raw = sample(bmp, u, v);
  int index = night ? s->NightMapIndex : s->DayMapIndex;
  if (index == 2) {
    /* Mask alpha only; retain exact resource class colors. */
    uint8_t rgb = raw & 0x3F;
    if (rgb == (GColorGreen.argb & 0x3F))
      return night ? s->NightLand : s->DayLand;
    if (rgb == (GColorCyan.argb & 0x3F))
      return night ? s->NightIce : s->DayIce;
    return night ? s->NightWater : s->DayWater;
  }
  return (GColor){.argb = raw | 0xC0};
#else
  /* Monochrome: invert the map at night, preserving geographic controls. */
  bool white = sample(s_world_bitmap, u, v) == 0xFF;
  return (white != night) ? GColorWhite : GColorBlack;
#endif
#endif
}
static void draw_watch(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect band = GRect(0, bounds.size.h*MAP_TOP_168/LAYOUT_H,
                    bounds.size.w, bounds.size.h*MAP_H_168/LAYOUT_H);
  ClaySettings *s = settings_get();
  /* Clear only the map band; text and battery remain outside it. */
  graphics_context_set_fill_color(ctx, s->BackgroundColor);
  graphics_fill_rect(ctx, band, 0, GCornerNone);
  if (!s_world_bitmap) return;
#ifdef PBL_COLOR
  if (!s_night_bitmap) return;
#endif
  float sun_lat, sun_lon;
  earth_sun(time(NULL) + s_time_offset, &sun_lat, &sun_lon);
  EarthVector sun = earth_vector(sun_lat, sun_lon);
  float view_lon = s->LongitudeOffset;
  if (s->CenterFocus == 1) view_lon = sun_lon;
  else if (s->CenterFocus == 2) view_lon = earth_wrap(sun_lon+180);
  APP_LOG(APP_LOG_LEVEL_INFO,"EARTH VIEW projection=%d focus=%d lat=%d lon=%d day=%d night=%d",
      s->MapProjection,s->CenterFocus,s->LatitudeOffset,s->LongitudeOffset,s->DayMapIndex,s->NightMapIndex);
  report_geometry();
  bool globe = s->MapProjection == MAP_PROJECTION_3D;
  GRect area = band;
  if (globe) {
    int d = (band.size.w < band.size.h ? band.size.w : band.size.h)-2;
    if (d < 2) return;
    area = GRect(band.origin.x+(band.size.w-d)/2,
                 band.origin.y+(band.size.h-d)/2, d, d);
  } else {
    /* Fit a true 2:1 world map into the available band. */
    int w = band.size.w;
    if (w > 2*band.size.h) w = 2*band.size.h;
    w -= w%2;
    area = GRect(band.origin.x+(band.size.w-w)/2,
                 band.origin.y+(band.size.h-w/2)/2, w, w/2);
  }
  if (area.size.w < 2 || area.size.h < 1 || area.size.w > MAX_MAP_WIDTH) return;
  EarthCamera camera = earth_camera(s->LatitudeOffset, view_lon);
  float radius = area.size.w*0.5f;
  /* 2 marks pixels outside the previous row's globe silhouette. */
  memset(s_previous_row, 2, sizeof(s_previous_row));
  for (int y = 0; y < area.size.h; ++y) {
    int left = 2;
    for (int x = 0; x < area.size.w; ++x) {
      EarthVector p;
      float u, v;
      if (globe) {
        float sx = (x+0.5f-radius)/radius;
        float sy = (radius-y-0.5f)/radius;
        if (!earth_surface(camera, sx, sy, &p)) {
          s_previous_row[x] = 2;
          left = 2;
          continue;
        }
        earth_uv(p, &u, &v);
      } else {
        float lon = earth_wrap(view_lon + ((x+0.5f)/area.size.w-0.5f)*360);
        float lat = 90.0f-(y+0.5f)/area.size.h*180.0f;
        u = (lon+180)/360;
        v = (90-lat)/180;
        p = earth_vector(lat, lon);
      }
      int night = earth_dot(p, sun) < 0;
      bool edge = (left != 2 && left != night)
               || (s_previous_row[x] != 2 && s_previous_row[x] != night);
      GColor color = pixel_color(s, u, v, night);
#if TERMINATOR_LINE && MAP_DEBUG_MODE == 0
      if (edge) {
#ifdef PBL_COLOR
        color = GColorLightGray;
#else
        color = GColorWhite;
#endif
      }
#else
      (void)edge;
#endif
      graphics_context_set_stroke_color(ctx, color);
      graphics_draw_pixel(ctx, GPoint(area.origin.x+x, area.origin.y+y));
      s_previous_row[x] = night;
      left = night;
    }
  }
}
void map_init(Layer *parent_layer, GRect bounds) {
  s_canvas = layer_create(bounds);
  if (!s_canvas) return;
  layer_set_update_proc(s_canvas, draw_watch);
  layer_add_child(parent_layer, s_canvas);
}
void map_deinit(void) {
  if (s_canvas) layer_destroy(s_canvas);
  s_canvas = NULL;
  if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
  s_world_bitmap = NULL;
#ifdef PBL_COLOR
  if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
  s_night_bitmap = NULL;
#endif
  s_day_index = s_night_index = -1;
}
void map_set_time_offset(int offset) { s_time_offset = offset; }
void map_force_redraw(void) { if (s_canvas) layer_mark_dirty(s_canvas); }
void map_reload_bitmaps(int day_index, int night_index) {
#ifdef PBL_COLOR
  if (day_index != s_day_index || !s_world_bitmap) {
    if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
    uint32_t id = RESOURCE_ID_DAY_01_CHARLIE;
    if (day_index == 1) id = RESOURCE_ID_DAY_02_BLUE_MARBLE;
    if (day_index == 2) id = RESOURCE_ID_3_Color_Map;
    s_world_bitmap = gbitmap_create_with_resource(id);
    s_day_index = day_index;
    report_bitmap("DAY",s_world_bitmap);
  }
  if (night_index != s_night_index || !s_night_bitmap) {
    if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
    uint32_t id = RESOURCE_ID_NIGHT_01_DITHER;
    if (night_index == 1) id = RESOURCE_ID_NIGHT_02_CLEAN;
    if (night_index == 2) id = RESOURCE_ID_3_Color_Map;
    s_night_bitmap = gbitmap_create_with_resource(id);
    s_night_index = night_index;
    report_bitmap("NIGHT",s_night_bitmap);
  }
  if (!s_world_bitmap || !s_night_bitmap)
    APP_LOG(APP_LOG_LEVEL_ERROR, "Earth bitmap allocation failed");
#else
  (void)day_index; (void)night_index;
  if (!s_world_bitmap) s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_WORLD);
#endif
}


// #include <pebble.h>
// #include "map.h"
// #include "layout.h"
// #include "settings.h"

// #define TERMINATOR_LINE 1
// #define TERMINATOR_PX   GColorLightGrayARGB8
// #define TERM_MAX_W      256

// static Layer *s_canvas;
// static GBitmap *s_world_bitmap;
// static int s_time_offset = 0;

// #ifdef PBL_COLOR
// static GBitmap *s_night_bitmap;
// static uint8_t s_term_flags[2][TERM_MAX_W];
// #endif

// // --- Math functions ---
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
//       // 1-bit native Pebble bitmaps are LSB first
//       return (row[x >> 3] & (1 << (x & 7))) ? 0xFF : 0x00;

//     case GBitmapFormat1BitPalette:
//       // 1-bit palette is MSB first
//       return (uint8_t)gbitmap_get_palette(bmp)[(row[x >> 3] >> (7 - (x & 7))) & 0x01].argb;

//     case GBitmapFormat2BitPalette:
//       // 2-bit palette is MSB first: shifts by 6, 4, 2, 0
//       return (uint8_t)gbitmap_get_palette(bmp)[(row[x >> 2] >> (6 - 2 * (x & 3))) & 0x03].argb;

//     case GBitmapFormat4BitPalette:
//       // 4-bit palette is MSB first: shifts by 4, 0
//       return (uint8_t)gbitmap_get_palette(bmp)[(row[x >> 1] >> (4 * (1 - (x & 1)))) & 0x0F].argb;

//     default:
//       return 0xC0; // Pure Black fallback
//   }
// }
// typedef struct { int x, y, w, h; } SrcRect;
// static SrcRect globe_src(GBitmap *bmp) {
//   GSize s2 = gbitmap_get_bounds(bmp).size;
//   return (SrcRect){ 0, 0, s2.w, s2.h };
// }
// #endif

// // --- Drawing logic ---
// static void draw_watch(struct Layer *layer, GContext *ctx) {
//   GRect bounds = layer_get_bounds(layer);
//   GRect map = GRect(0, (bounds.size.h * MAP_TOP_168) / LAYOUT_H, bounds.size.w, (bounds.size.h * MAP_H_168) / LAYOUT_H);
//   int sun_x, sun_y;
//   sun_position((int)time(NULL) + s_time_offset, &sun_x, &sun_y);

// #ifdef PBL_COLOR
//   GBitmap *fb = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
//   if (!fb) return;

//   uint8_t *fb_data = gbitmap_get_data(fb);
//   int fb_stride = gbitmap_get_bytes_per_row(fb);

//   GSize dsz = gbitmap_get_bounds(s_world_bitmap).size;
//   const uint8_t *ddata = gbitmap_get_data(s_world_bitmap);
//   int dstride = gbitmap_get_bytes_per_row(s_world_bitmap);

//   GSize nsz = gbitmap_get_bounds(s_night_bitmap).size;
//   const uint8_t *ndata = gbitmap_get_data(s_night_bitmap);
//   int nstride = gbitmap_get_bytes_per_row(s_night_bitmap);

//   SrcRect dsrc = globe_src(s_world_bitmap);
//   SrcRect nsrc = globe_src(s_night_bitmap);

//   int d_drawn_h = (dsrc.h * map.size.w) / dsrc.w;
//   int n_drawn_h = (nsrc.h * map.size.w) / nsrc.w;
//   if (d_drawn_h < 1) d_drawn_h = 1;
//   if (n_drawn_h < 1) n_drawn_h = 1;

//   int cols = map.size.w;
//   if (cols > TERM_MAX_W) cols = TERM_MAX_W;

//   // --- DYNAMIC SHIFT MATH ---
//   ClaySettings *settings = settings_get();
//   int screen_shift = 0;

//   if (settings->CenterFocus == 0) {
//     // 0: Fixed Longitude (Slider)
//     screen_shift = (settings->LongitudeOffset * map.size.w) / 360;
//   } else {
//     // Convert sun position to degrees (0 to 360)
//     int sun_deg = (sun_x * 360) / TRIG_MAX_ANGLE;
    
//     if (settings->CenterFocus == 1) {
//       // 1: Center Day (Put sun in the middle of the screen)
//       int shift_deg = sun_deg - 180; 
//       screen_shift = (shift_deg * map.size.w) / 360;
//     } 
//     else if (settings->CenterFocus == 2) {
//       // 2: Center Night (Put sun at the edges, night in the middle)
//       int shift_deg = sun_deg; 
//       screen_shift = (shift_deg * map.size.w) / 360;
//     }
//   }

//   // Ensure positive wrap around for modulo calculation
//   while (screen_shift < 0) {
//     screen_shift += map.size.w;
//   }
//   screen_shift = screen_shift % map.size.w;

//   // --- RENDER LOOP ---
//   for (int dy = 0; dy < map.size.h; dy++) {
//     int d_sy = dsrc.y + (dy * dsrc.h) / d_drawn_h;
//     int n_sy = nsrc.y + (dy * nsrc.h) / n_drawn_h;
//     if (d_sy >= dsrc.y + dsrc.h) d_sy = dsrc.y + dsrc.h - 1;
//     if (n_sy >= nsrc.y + nsrc.h) n_sy = nsrc.y + nsrc.h - 1;

//     const uint8_t *drow = ddata + d_sy * dstride;
//     const uint8_t *nrow = ndata + n_sy * nstride;
//     uint8_t *fb_row = fb_data + (map.origin.y + dy) * fb_stride + map.origin.x;
//     uint8_t *cur_flags = s_term_flags[dy & 1];
//     uint8_t *prev_flags = s_term_flags[(dy + 1) & 1];
//     int prev_night = -1;

//     for (int dx = 0; dx < cols; dx++) {
      
//       // Calculate shifted X coordinate
//       int map_dx = (dx + screen_shift) % map.size.w;
      
//       int night = is_night(map_dx, dy, map, sun_x, sun_y) ? 1 : 0;
//       bool on_seam = false;

// #if TERMINATOR_LINE
//       if (prev_night >= 0 && night != prev_night) on_seam = true;
//       if (dy > 0 && prev_flags[dx] != night) on_seam = true;
// #endif

//       uint8_t px;
//       if (on_seam) {
//         px = TERMINATOR_PX;
//       } else {
//         bool use_custom_day = (!night && settings->DayMapIndex == 2);
//         bool use_custom_night = (night && settings->NightMapIndex == 2);
        
//         // --- CUSTOM 3-COLOR MAP LOGIC ---
//         if (use_custom_day || use_custom_night) {
          
//           // Read from the correct bitmap (Day or Night)
//           uint8_t raw_px = night ? read_px(s_night_bitmap, nrow, (map_dx * nsz.w) / map.size.w) 
//                                  : read_px(s_world_bitmap, drow, (map_dx * dsz.w) / map.size.w);
          
//           // Compare directly using Pebble's raw ARGB byte codes
//           if (raw_px == GColorGreen.argb) {
//             px = night ? settings->NightLand.argb : settings->DayLand.argb;
//           } else if (raw_px == GColorCyan.argb) {
//             px = night ? settings->NightIce.argb : settings->DayIce.argb;
//           } else {
//             // Default fallback is water
//             px = night ? settings->NightWater.argb : settings->DayWater.argb;
//           }

//         } else {
//           // --- STANDARD MAP LOGIC ---
//           px = night ? read_px(s_night_bitmap, nrow, (map_dx * nsz.w) / map.size.w) 
//                      : read_px(s_world_bitmap, drow, (map_dx * dsz.w) / map.size.w);
//         }
//       }

//       // Ensure the pixel is fully opaque
//       if ((px & 0xC0) == 0) px = 0xC0;
//       fb_row[dx] = px | 0xC0;
      
//       // Save original dx flags for seam calculation
//       cur_flags[dx] = (uint8_t)night;
//       prev_night = night;
//     }
//   }
//   graphics_release_frame_buffer(ctx, fb);
// #else
//   GSize size = gbitmap_get_bounds(s_world_bitmap).size;
//   graphics_draw_bitmap_in_rect(ctx, s_world_bitmap, GRect(map.origin.x, map.origin.y, size.w, size.h));
// #endif
// }

// void map_init(Layer *parent_layer, GRect bounds) {
//   s_canvas = layer_create(bounds);
//   layer_set_update_proc(s_canvas, draw_watch);
//   layer_add_child(parent_layer, s_canvas);
// }

// void map_deinit(void) {
//   layer_destroy(s_canvas);
//   if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
// #ifdef PBL_COLOR
//   if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
// #endif
// }

// void map_set_time_offset(int offset) {
//   s_time_offset = offset;
// }

// void map_force_redraw(void) {
//   layer_mark_dirty(s_canvas);
// }

// void map_reload_bitmaps(int day_index, int night_index) {
//   if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
// #ifdef PBL_COLOR
//   if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
  
//   // --- Load Day Map ---
//   switch (day_index) {
//     case 1:
//       s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DAY_02_BLUE_MARBLE);
//       break;
//     case 2:
//       s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_3_Color_Map); 
//       break;
// //     case 3:
// //       s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DAY_04_OPTION); 
// //       break;
// //     case 4:
// //       s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DAY_05_OPTION); 
// //       break;
//     case 0:
//     default:
//       s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DAY_01_CHARLIE);
//       break;
//   }

//   // --- Load Night Map ---
//   switch (night_index) {
//     case 1:
//       s_night_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NIGHT_02_CLEAN);
//       break;
//     case 2:
//       s_night_bitmap = gbitmap_create_with_resource(RESOURCE_ID_3_Color_Map); 
//       break;
// //     case 3:
// //       s_night_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NIGHT_04_OPTION); 
// //       break;
// //     case 4:
// //       s_night_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NIGHT_05_OPTION); 
// //       break;
//     case 0:
//     default:
//       s_night_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NIGHT_01_DITHER);
//       break;
//   }
// #else
//   // Monochrome watches only have one map
//   s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_WORLD);
// #endif
// }