#include "color.h"

struct Color color_from_hex(uint32_t hex) {
    return (struct Color){
        .r = (hex >> 16) / 255.0f,
        .g = ((hex >> 8) & 0xff) / 255.0f,
        .b = (hex & 0xff) / 255.0f,
    };
}