#pragma once
#include <stdint.h>

// wrap acts like modulo, when y <= x < 2y, by subtracting y from x
uint32_t wrap(uint32_t x, uint32_t y);

uint32_t modulo(int32_t x, int32_t y);
float lerp(float a, float b, float t);
float spline2(float a, float b, float c, float t);
