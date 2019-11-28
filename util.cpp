#include "util.h"

uint32_t modulo(int32_t x, int32_t y)
{
    int32_t result = x % y;
    if (result < 0) result += y;
    return result;
}
