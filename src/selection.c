#include "selection.h"

struct Selection selection_sorted(struct Selection *selection) {
    struct Selection sorted_selection = *selection;

    if (selection->end_y < selection->start_y ||
        (selection->end_y == selection->start_y && selection->end_x < selection->start_x)) {

        sorted_selection.start_x = selection->end_x;
        sorted_selection.start_y = selection->end_y;
        sorted_selection.end_x = selection->start_x;
        sorted_selection.end_y = selection->start_y;
    }

    return sorted_selection;
}

bool selection_contains_point(struct Selection *selection, int32_t x, int32_t y) {
    return (y >= selection->start_y && y <= selection->end_y) &&
           (y != selection->start_y || x >= selection->start_x) &&
           (y != selection->end_y || x <= selection->end_x);
}