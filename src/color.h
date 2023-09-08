#ifndef COLOR_H
#define COLOR_H

#include <inttypes.h>

struct Color {
    float r;
    float g;
    float b;
};

struct Color color_from_hex(uint32_t hex);

#endif