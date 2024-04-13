#ifndef SELECTION_H
#define SELECTION_H

#include <inttypes.h>
#include <stdbool.h>

struct Selection {
    uint32_t start_x;
    uint32_t start_y;
    uint32_t end_x;
    uint32_t end_y;
};

struct Selection selection_sorted(struct Selection *selection);
bool selection_contains_point(struct Selection *selection, uint32_t x, uint32_t y);

#endif