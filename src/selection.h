#ifndef SELECTION_H
#define SELECTION_H

#include <inttypes.h>
#include <stdbool.h>

enum SelectionState {
    SELECTION_STATE_NONE,
    SELECTION_STATE_STARTED,
    SELECTION_STATE_FINISHED,
};

struct Selection {
    int32_t start_x;
    int32_t start_y;
    int32_t end_x;
    int32_t end_y;
};

struct Selection selection_sorted(struct Selection *selection);
bool selection_contains_point(struct Selection *selection, int32_t x, int32_t y);

#endif