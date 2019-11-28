#pragma once
#include <stdint.h>

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

uint32_t modulo(int32_t x, int32_t y);
