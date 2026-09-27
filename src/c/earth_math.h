#pragma once
#include <math.h>
#include <stdbool.h>
#include <time.h>
#define EARTH_PI 3.14159265358979323846f
#define EARTH_RAD (EARTH_PI / 180.0f)
typedef struct { float x, y, z; } EarthVector;
typedef struct { EarthVector right, up, forward; } EarthCamera;
static inline float earth_wrap(float lon) {
  while (lon >= 180.0f) lon -= 360.0f;
  while (lon < -180.0f) lon += 360.0f;
  return lon;
}
static inline EarthVector earth_vector(float lat_deg, float lon_deg) {
  float lat = lat_deg * EARTH_RAD, lon = lon_deg * EARTH_RAD;
  float c = cosf(lat);
  return (EarthVector){c*cosf(lon), sinf(lat), c*sinf(lon)};
}
static inline float earth_dot(EarthVector a, EarthVector b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
}
static inline EarthCamera earth_camera(float lat_deg, float lon_deg) {
  float lat = lat_deg*EARTH_RAD, lon = lon_deg*EARTH_RAD;
  float sl = sinf(lat), cl = cosf(lat), so = sinf(lon), co = cosf(lon);
  return (EarthCamera){
    .right = {-so, 0, co},
    .up = {-sl*co, cl, -sl*so},
    .forward = {cl*co, sl, cl*so}
  };
}
/* sx and sy are screen-right and screen-up coordinates on a unit disk. */
static inline bool earth_surface(EarthCamera c, float sx, float sy, EarthVector *p) {
  float r2 = sx*sx + sy*sy;
  if (r2 > 1.0f) return false;
  float z = sqrtf(fmaxf(0.0f, 1.0f-r2));
  *p = (EarthVector){
    sx*c.right.x + sy*c.up.x + z*c.forward.x,
    sx*c.right.y + sy*c.up.y + z*c.forward.y,
    sx*c.right.z + sy*c.up.z + z*c.forward.z
  };
  return true;
}
/* Avoid atan2f: the on-watch diagnostic returned an invalid longitude.
 * Reduce atan's argument to |t| <= tan(pi/8), then evaluate an odd series.
 * This uses only ordinary float arithmetic, with explicit quadrant handling.
 */
static inline float earth_atan_unit(float t) {
  float shift = 0.0f;
  if (t > 0.41421356237f) {
    t = (t - 1.0f) / (t + 1.0f);
    shift = EARTH_PI * 0.25f;
  }
  float t2 = t*t, term = t, sum = t;
  for (int n = 1; n <= 9; ++n) {
    term *= -t2;
    sum += term / (2*n + 1);
  }
  return shift + sum;
}
static inline float earth_atan2(float y, float x) {
  float ax = x < 0 ? -x : x;
  float ay = y < 0 ? -y : y;
  if (ax == 0.0f && ay == 0.0f) return 0.0f;
  float angle;
  if (ax >= ay) angle = earth_atan_unit(ay/ax);
  else angle = EARTH_PI*0.5f - earth_atan_unit(ax/ay);
  if (x < 0.0f) angle = EARTH_PI - angle;
  if (y < 0.0f) angle = -angle;
  return angle;
}
static inline void earth_uv(EarthVector p, float *u, float *v) {
  float y = fmaxf(-1.0f, fminf(1.0f, p.y));
  *u = (earth_atan2(p.z, p.x) + EARTH_PI) / (2.0f*EARTH_PI);
  if (*u >= 1.0f) *u = 0.0f;
  *v = (EARTH_PI*0.5f - asinf(y)) / EARTH_PI;
}
/* Compact fractional-year solar approximation, evaluated in UTC. */
static inline void earth_sun(time_t now, float *lat, float *lon) {
  struct tm *utc = gmtime(&now);
  if (!utc) { *lat = 0; *lon = 0; return; }
  int year = utc->tm_year + 1900;
  int days = (year%4 == 0 && (year%100 != 0 || year%400 == 0)) ? 366 : 365;
  float hour = utc->tm_hour + utc->tm_min/60.0f + utc->tm_sec/3600.0f;
  float g = 2.0f*EARTH_PI/days * (utc->tm_yday+(hour-12.0f)/24.0f);
  float decl = .006918f-.399912f*cosf(g)+.070257f*sinf(g)
    -.006758f*cosf(2*g)+.000907f*sinf(2*g)
    -.002697f*cosf(3*g)+.001480f*sinf(3*g);
  float eq = 229.18f*(.000075f+.001868f*cosf(g)-.032077f*sinf(g)
    -.014615f*cosf(2*g)-.040849f*sinf(2*g));
  *lat = decl/EARTH_RAD;
  *lon = earth_wrap((720.0f-hour*60.0f-eq)/4.0f);
}
