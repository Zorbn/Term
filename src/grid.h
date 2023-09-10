#ifndef GRID_H
#define GRID_H

#include "text_buffer.h"
#include "graphics/sprite_batch.h"

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#define GRID_COLOR_BACKGROUND_DEFAULT 0x0c0c0c
#define GRID_COLOR_FOREGROUND_DEFAULT 0xcccccc

#define GRID_COLOR_BLACK 0x0c0c0c
#define GRID_COLOR_RED 0xc50f1f
#define GRID_COLOR_GREEN 0x13a10e
#define GRID_COLOR_YELLOW 0xc19c00
#define GRID_COLOR_BLUE 0x0037da
#define GRID_COLOR_MAGENTA 0x881798
#define GRID_COLOR_CYAN 0x3a96dd
#define GRID_COLOR_WHITE 0xcccccc

#define GRID_COLOR_BRIGHT_BLACK 0x767676
#define GRID_COLOR_BRIGHT_RED 0xe74856
#define GRID_COLOR_BRIGHT_GREEN 0x16c60c
#define GRID_COLOR_BRIGHT_YELLOW 0xf9f1a5
#define GRID_COLOR_BRIGHT_BLUE 0x3b78ff
#define GRID_COLOR_BRIGHT_MAGENTA 0xb4009e
#define GRID_COLOR_BRIGHT_CYAN 0x61d6d6
#define GRID_COLOR_BRIGHT_WHITE 0xf2f2f2

struct Window;
void window_set_title(struct Window *window, char *title);

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

    bool should_show_cursor;
    bool should_send_mouse_inputs;

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
bool grid_parse_escape_sequence(
    struct Grid *grid, struct TextBuffer *text_buffer, size_t *i, size_t *furthest_i, struct Window *window);
void grid_draw_tile(struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, float scale);
void grid_draw_cursor(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, float scale);
void grid_destroy(struct Grid *grid);

inline void grid_set_char_i(struct Grid *grid, int32_t i, char character) {
    grid->data[i] = character;
    grid->background_colors[i] = grid->current_background_color;
    grid->foreground_colors[i] = grid->current_foreground_color;

    grid->are_rows_dirty[i / grid->width] = true;
}

#endif