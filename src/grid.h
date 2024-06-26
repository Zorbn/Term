#ifndef GRID_H
#define GRID_H

#include "window.h"
#include "text_buffer.h"
#include "list.h"

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

struct TitleBuffer {
    char data[256];
    bool is_dirty;
};

enum GridCursorStyle {
    GRID_CURSOR_STYLE_BLOCK,
    GRID_CURSOR_STYLE_UNDERLINE,
    GRID_CURSOR_STYLE_BAR,
};

enum GridMouseMode {
    GRID_MOUSE_MODE_NONE,
    GRID_MOUSE_MODE_BUTTON,
    GRID_MOUSE_MODE_DRAG,
    GRID_MOUSE_MODE_ANY,
};

struct ScrollbackLine {
    uint32_t *data;
    uint32_t *background_colors;
    uint32_t *foreground_colors;
    size_t length;
};

typedef struct ScrollbackLine struct_ScrollbackLine;
LIST_DEFINE(struct_ScrollbackLine)

struct Grid {
    uint32_t *data;
    size_t width;
    size_t height;
    size_t size;

    struct List_struct_ScrollbackLine scrollback_lines;

    int32_t cursor_x;
    int32_t cursor_y;

    int32_t saved_cursor_x;
    int32_t saved_cursor_y;

    bool should_show_cursor;
    bool should_use_sgr_format;
    bool has_mouse_mode_button;
    bool has_mouse_mode_drag;
    bool has_mouse_mode_any;

    enum GridCursorStyle cursor_style;

    uint32_t *background_colors;
    uint32_t *foreground_colors;

    uint32_t current_background_color;
    uint32_t current_foreground_color;

    bool are_colors_swapped;

    void *callback_context;
    void (*on_row_changed)(void *context, int32_t y);
};

struct Grid grid_create(
    size_t width, size_t height, void *callback_context, void (*on_row_changed)(void *context, int32_t y));
void grid_resize(struct Grid *grid, size_t width, size_t height);
void grid_set_char(struct Grid *grid, int32_t x, int32_t y, uint32_t character);
void grid_set_cursor_style(struct Grid *grid, enum GridCursorStyle cursor_style);
void grid_scroll_down(struct Grid *grid);
void grid_cursor_move_to(struct Grid *grid, int32_t x, int32_t y);
void grid_cursor_move(struct Grid *grid, int32_t delta_x, int32_t delta_y);
enum GridMouseMode grid_get_mouse_mode(struct Grid *grid);
bool grid_parse_escape_sequence(
    struct Grid *grid, struct TextBuffer *text_buffer, struct TitleBuffer *title_buffer, size_t *i, size_t *furthest_i);
void grid_destroy(struct Grid *grid);

inline void grid_set_char_i(struct Grid *grid, int32_t i, uint32_t character) {
    grid->data[i] = character;
    grid->background_colors[i] = grid->current_background_color;
    grid->foreground_colors[i] = grid->current_foreground_color;

    grid->on_row_changed(grid->callback_context, i / grid->width);
}

#endif