#include <pebble.h>
#include "map.h"
#include "layout.h"
#include "settings.h"
#include "earth_math.h"

/* 0: normal, 1: plain light/shadow, 2: day texture only. */
#define MAP_DEBUG_MODE 0
#define TERMINATOR_LINE 1

// Maximum physical screen width for Pebble (144 on Basalt/Aplite, 180 on Chalk, 200 on Emery)
#define MAX_MAP_WIDTH 200

static Layer *s_canvas;
static GBitmap *s_world_bitmap;
#ifdef PBL_COLOR
static GBitmap *s_night_bitmap;
#endif
static int s_day_index = -1, s_night_index = -1;
static int s_time_offset;
static uint8_t s_previous_row[MAX_MAP_WIDTH];

// 2D Fast Math Arrays (Small footprint: < 2 KB)
static float s_lon_c[MAX_MAP_WIDTH], s_lon_s[MAX_MAP_WIDTH];
static float s_lat_c[MAX_MAP_WIDTH], s_lat_s[MAX_MAP_WIDTH];
static uint8_t s_u_base[MAX_MAP_WIDTH], s_v[MAX_MAP_WIDTH];

static int s_cached_proj = -1;
static int s_cached_w = 0, s_cached_h = 0;

static void update_math_caches_2d(int proj, int w, int h) {
  if (proj == s_cached_proj && w == s_cached_w && h == s_cached_h) {
    return;
  }
  s_cached_proj = proj;
  s_cached_w = w;
  s_cached_h = h;

  if (proj == MAP_PROJECTION_2D) {
    int max_w = (w > MAX_MAP_WIDTH) ? MAX_MAP_WIDTH : w;
    int max_h = (h > MAX_MAP_WIDTH) ? MAX_MAP_WIDTH : h;

    for (int x = 0; x < max_w; ++x) {
      float lon = earth_wrap(((x + 0.5f) / w - 0.5f) * 360.0f);
      float u = (lon + 180.0f) / 360.0f;
      s_u_base[x] = (uint8_t)(u * 255.0f);
      s_lon_c[x] = cosf(lon * EARTH_RAD);
      s_lon_s[x] = sinf(lon * EARTH_RAD);
    }
    for (int y = 0; y < max_h; ++y) {
      float lat = 90.0f - (y + 0.5f) / h * 180.0f;
      float v = (90.0f - lat) / 180.0f;
      s_v[y] = (uint8_t)(v * 255.0f);
      s_lat_c[y] = cosf(lat * EARTH_RAD);
      s_lat_s[y] = sinf(lat * EARTH_RAD);
    }
  }
}

// ==============================================================================
// DRAWING HELPERS
// ==============================================================================

static uint8_t read_pixel(GBitmap *bmp, int x, int y) {
  if (!bmp) return 0xC0;
  const uint8_t *row = gbitmap_get_data(bmp) + y * gbitmap_get_bytes_per_row(bmp);
  switch (gbitmap_get_format(bmp)) {
    case GBitmapFormat1Bit:
      return (row[x >> 3] & (1u << (x & 7))) ? 0xFF : 0xC0;
#ifdef PBL_COLOR
    case GBitmapFormat8Bit: 
      return row[x];
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

static uint8_t sample(GBitmap *bmp, uint8_t u, uint8_t v) {
  if (!bmp) return 0xC0;
  GSize size = gbitmap_get_bounds(bmp).size;
  int x = (u * size.w) >> 8;
  int y = (v * size.h) >> 8;
  if (x < 0) x = 0;
  if (x >= size.w) x = size.w - 1;
  if (y < 0) y = 0;
  if (y >= size.h) y = size.h - 1;
  return read_pixel(bmp, x, y);
}

static GColor pixel_color(ClaySettings *s, uint8_t u, uint8_t v, bool night) {
#if MAP_DEBUG_MODE == 1
  return night ? GColorBlack : GColorWhite;
#else
#if MAP_DEBUG_MODE == 2
  night = false;
#endif
#ifdef PBL_COLOR
  GBitmap *bmp = (night && s_night_bitmap) ? s_night_bitmap : s_world_bitmap;
  uint8_t raw = sample(bmp, u, v);
  int index = night ? s->NightMapIndex : s->DayMapIndex;
  
  if (index == 2) {
    uint8_t rgb = raw & 0x3F;
    if (rgb == (GColorGreen.argb & 0x3F))
      return night ? s->NightLand : s->DayLand;
    if (rgb == (GColorCyan.argb & 0x3F))
      return night ? s->NightIce : s->DayIce;
    return night ? s->NightWater : s->DayWater;
  }
  return (GColor){.argb = raw | 0xC0};
#else
  bool white = sample(s_world_bitmap, u, v) == 0xFF;
  return (white != night) ? GColorWhite : GColorBlack;
#endif
#endif
}

// ==============================================================================
// CORE RENDERING LOOP
// ==============================================================================
static void draw_watch(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect band = GRect(0, bounds.size.h * MAP_TOP_168 / LAYOUT_H, bounds.size.w, bounds.size.h * MAP_H_168 / LAYOUT_H);
  ClaySettings *s = settings_get();
  
  graphics_context_set_fill_color(ctx, s->BackgroundColor);
  graphics_fill_rect(ctx, band, 0, GCornerNone);
  
  if (!s_world_bitmap) return;

  // 1. Calculate true sub-solar point
  float sun_lat, sun_lon;
  earth_sun(time(NULL) + s_time_offset, &sun_lat, &sun_lon);
  EarthVector sun = earth_vector(sun_lat, sun_lon);

  // 2. Determine View Lat & Lon based on CenterFocus setting
  float view_lat = (float)s->LatitudeOffset;
  float view_lon = (float)s->LongitudeOffset;

  if (s->CenterFocus == 1) {
    // Mode 1: Center on Day (Sub-solar point)
    view_lat = sun_lat;
    view_lon = sun_lon;
  } else if (s->CenterFocus == 2) {
    // Mode 2: Center on Night (Antipodal / Midnight point)
    view_lat = -sun_lat;
    view_lon = earth_wrap(sun_lon + 180.0f);
  }
  
  bool globe = (s->MapProjection == MAP_PROJECTION_3D);
  GRect area = band;
  
  if (globe) {
    int d = (band.size.w < band.size.h ? band.size.w : band.size.h) - 2;
    if (d < 2) return;
    area = GRect(band.origin.x + (band.size.w - d) / 2, band.origin.y + (band.size.h - d) / 2, d, d);
  } else {
    int w = band.size.w;
    if (w > 2 * band.size.h) w = 2 * band.size.h;
    w -= w % 2;
    area = GRect(band.origin.x + (band.size.w - w) / 2, band.origin.y + (band.size.h - w / 2) / 2, w, w / 2);
  }
  if (area.size.w < 2 || area.size.h < 1 || area.size.w > MAX_MAP_WIDTH) return;

  memset(s_previous_row, 2, sizeof(s_previous_row));

  if (globe) {
    // =========================================================================
    // 3D GLOBE RENDERING
    // =========================================================================
    int radius = area.size.w / 2;
    int diameter = radius * 2;
    EarthCamera camera = earth_camera(view_lat, view_lon);

    for (int y = 0; y < diameter; ++y) {
      int left = 2;
      float sy = (radius - y - 0.5f) / (float)radius;

      for (int x = 0; x < diameter; ++x) {
        float sx = (x + 0.5f - radius) / (float)radius;
        EarthVector p;

        if (earth_surface(camera, sx, sy, &p)) {
          float u_flt, v_flt;
          earth_uv(p, &u_flt, &v_flt);

          uint8_t u_coord = (uint8_t)(u_flt * 255.0f);
          uint8_t v_coord = (uint8_t)(v_flt * 255.0f);

          // Dot product between surface normal and Sun vector
          float dot = (p.x * sun.x) + (p.y * sun.y) + (p.z * sun.z);
          int night = (dot < 0);

          bool edge = (left != 2 && left != night)
                   || (s_previous_row[x] != 2 && s_previous_row[x] != night);

          GColor color = pixel_color(s, u_coord, v_coord, night);

#if TERMINATOR_LINE && MAP_DEBUG_MODE == 0
          if (edge) {
#ifdef PBL_COLOR
            color = GColorLightGray;
#else
            color = GColorWhite;
#endif
          }
#endif
          graphics_context_set_stroke_color(ctx, color);
          graphics_draw_pixel(ctx, GPoint(area.origin.x + x, area.origin.y + y));

          s_previous_row[x] = night;
          left = night;
        } else {
          s_previous_row[x] = 2;
          left = 2;
        }
      }
    }
  } else {
    // =========================================================================
    // 2D MAP RENDERING
    // =========================================================================
    float dot_x[MAX_MAP_WIDTH];
    uint8_t u_coords[MAX_MAP_WIDTH];

    for (int x = 0; x < area.size.w; ++x) {
      float lon = earth_wrap(((x + 0.5f) / (float)area.size.w - 0.5f) * 360.0f + view_lon);
      float u = (lon + 180.0f) / 360.0f;
      u_coords[x] = (uint8_t)(u * 255.0f);
      dot_x[x] = sun.x * cosf(lon * EARTH_RAD) + sun.z * sinf(lon * EARTH_RAD);
    }

    for (int y = 0; y < area.size.h; ++y) {
      int left = 2;
      float lat = 90.0f - (y + 0.5f) / (float)area.size.h * 180.0f;
      float v = (90.0f - lat) / 180.0f;
      uint8_t v_coord = (uint8_t)(v * 255.0f);

      float lat_rad = lat * EARTH_RAD;
      float c_lat = cosf(lat_rad);
      float dot_y_const = sun.y * sinf(lat_rad);

      for (int x = 0; x < area.size.w; ++x) {
        float dot = c_lat * dot_x[x] + dot_y_const;
        int night = (dot < 0);

        bool edge = (left != 2 && left != night)
                 || (s_previous_row[x] != 2 && s_previous_row[x] != night);

        GColor color = pixel_color(s, u_coords[x], v_coord, night);

#if TERMINATOR_LINE && MAP_DEBUG_MODE == 0
        if (edge) {
#ifdef PBL_COLOR
          color = GColorLightGray;
#else
          color = GColorWhite;
#endif
        }
#endif
        graphics_context_set_stroke_color(ctx, color);
        graphics_draw_pixel(ctx, GPoint(area.origin.x + x, area.origin.y + y));

        s_previous_row[x] = night;
        left = night;
      }
    }
  }
}

// static void draw_watch(Layer *layer, GContext *ctx) {
//   GRect bounds = layer_get_bounds(layer);
//   GRect band = GRect(0, bounds.size.h * MAP_TOP_168 / LAYOUT_H, bounds.size.w, bounds.size.h * MAP_H_168 / LAYOUT_H);
//   ClaySettings *s = settings_get();
  
//   graphics_context_set_fill_color(ctx, s->BackgroundColor);
//   graphics_fill_rect(ctx, band, 0, GCornerNone);
  
//   if (!s_world_bitmap) return;

//   // 1. Calculate the sun's exact sub-solar point (latitude & longitude)
//   float sun_lat, sun_lon;
//   earth_sun(time(NULL) + s_time_offset, &sun_lat, &sun_lon);
//   EarthVector sun = earth_vector(sun_lat, sun_lon);

//   // 2. Determine View Latitude & Longitude based on CenterFocus setting
//   float view_lat = (float)s->LatitudeOffset;
//   float view_lon = (float)s->LongitudeOffset;

//   if (s->CenterFocus == 1) {
//     // Mode 1: Center on Day (Sun directly overhead in the center)
//     view_lat = sun_lat;
//     view_lon = sun_lon;
//   } else if (s->CenterFocus == 2) {
//     // Mode 2: Center on Night (Solar Midnight directly in the center)
//     view_lat = -sun_lat;
//     view_lon = earth_wrap(sun_lon + 180.0f);
//   }
  
//   bool globe = (s->MapProjection == MAP_PROJECTION_3D);
//   GRect area = band;
  
//   if (globe) {
//     int d = (band.size.w < band.size.h ? band.size.w : band.size.h) - 2;
//     if (d < 2) return;
//     area = GRect(band.origin.x + (band.size.w - d) / 2, band.origin.y + (band.size.h - d) / 2, d, d);
//   } else {
//     int w = band.size.w;
//     if (w > 2 * band.size.h) w = 2 * band.size.h;
//     w -= w % 2;
//     area = GRect(band.origin.x + (band.size.w - w) / 2, band.origin.y + (band.size.h - w / 2) / 2, w, w / 2);
//   }
//   if (area.size.w < 2 || area.size.h < 1 || area.size.w > MAX_MAP_WIDTH) return;

//   memset(s_previous_row, 2, sizeof(s_previous_row));

//   if (globe) {
//     // -------------------------------------------------------------
//     // 3D GLOBE RENDERING
//     // Point the camera directly at the chosen (view_lat, view_lon)
//     // -------------------------------------------------------------
//     int radius = area.size.w / 2;
//     int diameter = radius * 2;
//     EarthCamera camera = earth_camera(view_lat, view_lon);

//     for (int y = 0; y < diameter; ++y) {
//       int left = 2;
//       float sy = (radius - y - 0.5f) / (float)radius;

//       for (int x = 0; x < diameter; ++x) {
//         float sx = (x + 0.5f - radius) / (float)radius;
//         EarthVector p;

//         if (earth_surface(camera, sx, sy, &p)) {
//           // Texture coordinates natively derived from 3D surface point
//           float u_flt, v_flt;
//           earth_uv(p, &u_flt, &v_flt);

//           uint8_t u_coord = (uint8_t)(u_flt * 255.0f);
//           uint8_t v_coord = (uint8_t)(v_flt * 255.0f);

//           // Dot product: > 0 is Daylight, < 0 is Night shadow
//           float dot = (p.x * sun.x) + (p.y * sun.y) + (p.z * sun.z);
//           int night = (dot < 0);

//           bool edge = (left != 2 && left != night)
//                    || (s_previous_row[x] != 2 && s_previous_row[x] != night);

//           GColor color = pixel_color(s, u_coord, v_coord, night);

// #if TERMINATOR_LINE && MAP_DEBUG_MODE == 0
//           if (edge) {
// #ifdef PBL_COLOR
//             color = GColorLightGray;
// #else
//             color = GColorWhite;
// #endif
//           }
// #endif
//           graphics_context_set_stroke_color(ctx, color);
//           graphics_draw_pixel(ctx, GPoint(area.origin.x + x, area.origin.y + y));

//           s_previous_row[x] = night;
//           left = night;
//         } else {
//           s_previous_row[x] = 2; // Outside sphere
//           left = 2;
//         }
//       }
//     }
//   } else {
//     // -------------------------------------------------------------
//     // 2D MAP RENDERING
//     // Both texture and day/night calculations use centered view_lon
//     // -------------------------------------------------------------
//     float dot_x[MAX_MAP_WIDTH];
//     uint8_t u_coords[MAX_MAP_WIDTH];

//     for (int x = 0; x < area.size.w; ++x) {
//       float lon = earth_wrap(((x + 0.5f) / (float)area.size.w - 0.5f) * 360.0f + view_lon);
//       float u = (lon + 180.0f) / 360.0f;
//       u_coords[x] = (uint8_t)(u * 255.0f);
//       dot_x[x] = sun.x * cosf(lon * EARTH_RAD) + sun.z * sinf(lon * EARTH_RAD);
//     }

//     for (int y = 0; y < area.size.h; ++y) {
//       int left = 2;
//       float lat = 90.0f - (y + 0.5f) / (float)area.size.h * 180.0f;
//       float v = (90.0f - lat) / 180.0f;
//       uint8_t v_coord = (uint8_t)(v * 255.0f);

//       float lat_rad = lat * EARTH_RAD;
//       float c_lat = cosf(lat_rad);
//       float dot_y_const = sun.y * sinf(lat_rad);

//       for (int x = 0; x < area.size.w; ++x) {
//         float dot = c_lat * dot_x[x] + dot_y_const;
//         int night = (dot < 0);

//         bool edge = (left != 2 && left != night)
//                  || (s_previous_row[x] != 2 && s_previous_row[x] != night);

//         GColor color = pixel_color(s, u_coords[x], v_coord, night);

// #if TERMINATOR_LINE && MAP_DEBUG_MODE == 0
//         if (edge) {
// #ifdef PBL_COLOR
//           color = GColorLightGray;
// #else
//           color = GColorWhite;
// #endif
//         }
// #endif
//         graphics_context_set_stroke_color(ctx, color);
//         graphics_draw_pixel(ctx, GPoint(area.origin.x + x, area.origin.y + y));

//         s_previous_row[x] = night;
//         left = night;
//       }
//     }
//   }
// }

// ==============================================================================
// INITIALIZATION / LIFECYCLE
// ==============================================================================

void map_init(Layer *parent_layer, GRect bounds) {
  s_canvas = layer_create(bounds);
  if (!s_canvas) return;
  
  layer_set_update_proc(s_canvas, draw_watch);
  layer_add_child(parent_layer, s_canvas);
}

void map_deinit(void) {
  if (s_canvas) {
    layer_destroy(s_canvas);
    s_canvas = NULL;
  }
  
  s_cached_proj = -1;

  if (s_world_bitmap) {
    gbitmap_destroy(s_world_bitmap);
    s_world_bitmap = NULL;
  }
  
#ifdef PBL_COLOR
  if (s_night_bitmap) {
    gbitmap_destroy(s_night_bitmap);
    s_night_bitmap = NULL;
  }
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
  }
  if (night_index != s_night_index || !s_night_bitmap) {
    if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
    uint32_t id = RESOURCE_ID_NIGHT_01_DITHER;
    if (night_index == 1) id = RESOURCE_ID_NIGHT_02_CLEAN;
    if (night_index == 2) id = RESOURCE_ID_3_Color_Map;
    s_night_bitmap = gbitmap_create_with_resource(id);
    s_night_index = night_index;
  }
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