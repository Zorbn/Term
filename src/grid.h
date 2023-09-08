#ifndef GRID_H
#define GRID_H

#include "window.h"
#include "text_buffer.h"
#include "graphics/sprite_batch.h"

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

struct Grid {
    char *data;
    bool *are_rows_dirty;
    size_t width;
    size_t height;
    size_t size;

    int32_t cursor_x;
    int32_t cursor_y;

    int32_t saved_cursor_x;
    int32_t saved_cursor_y;

    bool show_cursor;

    uint32_t *background_colors;
    uint32_t *foreground_colors;

    uint32_t current_background_color;
    uint32_t current_foreground_color;

    bool are_colors_swapped;
};

struct Grid grid_create(size_t width, size_t height);
void grid_resize(struct Grid *grid, size_t width, size_t height);
void grid_set_char(struct Grid *grid, int32_t x, int32_t y, char character);
void grid_scroll_down(struct Grid *grid);
void grid_cursor_move_to(struct Grid *grid, int32_t x, int32_t y);
void grid_cursor_move(struct Grid *grid, int32_t delta_x, int32_t delta_y);
bool grid_parse_escape_sequence(struct Grid *grid, struct TextBuffer *data, size_t *i, struct Window *window);
void grid_draw_tile(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z);
void grid_draw_cursor(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z);
void grid_destroy(struct Grid *grid);

inline void grid_set_char_i(struct Grid *grid, int32_t i, char character) {
    grid->data[i] = character;
    grid->background_colors[i] = grid->current_background_color;
    grid->foreground_colors[i] = grid->current_foreground_color;

    grid->are_rows_dirty[i / grid->width] = true;
}

#endif