#include "util.h"
#include <math.h>

uint32_t modulo(int32_t x, int32_t y)
{
    int32_t result = x % y;
    if (result < 0) result += y;
    return result;
}

uint32_t wrap(uint32_t x, uint32_t y)
{
    if (x >= y) return x - y;
    return x;
}

float lerp(float a, float b, float t)
{
    return (1.0f - t) * a + t * b;
}

float spline2(float a, float b, float c, float t)
{
    return a * 0.5f * t * (t - 1) - b * (t - 1) * (t  + 1) + c * 0.5f * t * (t + 1);
}

float cplx_angle(float r, float i)
{
    return atan2f(i, r);
}

float cplx_mag(float r, float i)
{
    return sqrtf(r*r + i*i);
}

float princarg(float t)
{
    return fmodf(t + M_PI, 2 * M_PI) - M_PI;
}

void cplx_polar_to_rect(float *r, float *i, float mag, float angle)
{
    *r = mag * cosf(angle);
    *i = mag * sinf(angle);
}
