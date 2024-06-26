#include "grid.h"

#include "color.h"
#include "font.h"
#include "geometry.h"

#include <math.h>

const uint32_t color_table[256] = {
    0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0, 0x808080, 0xff0000, 0x00ff00,
    0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff, 0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000d7, 0x0000ff,
    0x005f00, 0x005f5f, 0x005f87, 0x005faf, 0x005fd7, 0x005fff, 0x008700, 0x00875f, 0x008787, 0x0087af, 0x0087d7,
    0x0087ff, 0x00af00, 0x00af5f, 0x00af87, 0x00afaf, 0x00afd7, 0x00afff, 0x00d700, 0x00d75f, 0x00d787, 0x00d7af,
    0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f, 0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff, 0x5f0000, 0x5f005f, 0x5f0087,
    0x5f00af, 0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87, 0x5f5faf, 0x5f5fd7, 0x5f5fff, 0x5f8700, 0x5f875f,
    0x5f8787, 0x5f87af, 0x5f87d7, 0x5f87ff, 0x5faf00, 0x5faf5f, 0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff, 0x5fd700,
    0x5fd75f, 0x5fd787, 0x5fd7af, 0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f, 0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
    0x870000, 0x87005f, 0x870087, 0x8700af, 0x8700d7, 0x8700ff, 0x875f00, 0x875f5f, 0x875f87, 0x875faf, 0x875fd7,
    0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af, 0x8787d7, 0x8787ff, 0x87af00, 0x87af5f, 0x87af87, 0x87afaf,
    0x87afd7, 0x87afff, 0x87d700, 0x87d75f, 0x87d787, 0x87d7af, 0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f, 0x87ff87,
    0x87ffaf, 0x87ffd7, 0x87ffff, 0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af, 0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f,
    0xaf5f87, 0xaf5faf, 0xaf5fd7, 0xaf5fff, 0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87d7, 0xaf87ff, 0xafaf00,
    0xafaf5f, 0xafaf87, 0xafafaf, 0xafafd7, 0xafafff, 0xafd700, 0xafd75f, 0xafd787, 0xafd7af, 0xafd7d7, 0xafd7ff,
    0xafff00, 0xafff5f, 0xafff87, 0xafffaf, 0xafffd7, 0xafffff, 0xd70000, 0xd7005f, 0xd70087, 0xd700af, 0xd700d7,
    0xd700ff, 0xd75f00, 0xd75f5f, 0xd75f87, 0xd75faf, 0xd75fd7, 0xd75fff, 0xd78700, 0xd7875f, 0xd78787, 0xd787af,
    0xd787d7, 0xd787ff, 0xd7af00, 0xd7af5f, 0xd7af87, 0xd7afaf, 0xd7afd7, 0xd7afff, 0xd7d700, 0xd7d75f, 0xd7d787,
    0xd7d7af, 0xd7d7d7, 0xd7d7ff, 0xd7ff00, 0xd7ff5f, 0xd7ff87, 0xd7ffaf, 0xd7ffd7, 0xd7ffff, 0xff0000, 0xff005f,
    0xff0087, 0xff00af, 0xff00d7, 0xff00ff, 0xff5f00, 0xff5f5f, 0xff5f87, 0xff5faf, 0xff5fd7, 0xff5fff, 0xff8700,
    0xff875f, 0xff8787, 0xff87af, 0xff87d7, 0xff87ff, 0xffaf00, 0xffaf5f, 0xffaf87, 0xffafaf, 0xffafd7, 0xffafff,
    0xffd700, 0xffd75f, 0xffd787, 0xffd7af, 0xffd7d7, 0xffd7ff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffd7,
    0xffffff, 0x080808, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e, 0x585858, 0x626262,
    0x6c6c6c, 0x767676, 0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e, 0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6, 0xd0d0d0,
    0xdadada, 0xe4e4e4, 0xeeeeee,
};

struct Grid grid_create(
    size_t width, size_t height, void *callback_context, void (*on_row_changed)(void *context, int32_t y)
) {

    size_t size = width * height;
    struct Grid grid = (struct Grid){
        .data = malloc(size * sizeof(uint32_t)),
        .width = width,
        .height = height,
        .size = size,

        .scrollback_lines = list_create_struct_ScrollbackLine(64),

        .background_colors = malloc(size * sizeof(uint32_t)),
        .foreground_colors = malloc(size * sizeof(uint32_t)),

        .current_background_color = GRID_COLOR_BACKGROUND_DEFAULT,
        .current_foreground_color = GRID_COLOR_FOREGROUND_DEFAULT,

        .callback_context = callback_context,
        .on_row_changed = on_row_changed,
    };
    assert(grid.data);

    for (size_t i = 0; i < size; i++) {
        grid_set_char_i(&grid, i, ' ');
    }

    return grid;
}

// Get the length of a line without trailing whitespace.
size_t grid_get_occupied_line_length(struct Grid *grid, size_t y) {
    size_t line_length = 0;
    for (int32_t x = grid->width - 1; x >= 0; x--) {
        int32_t i = x + y * grid->width;

        if (grid->data[i] != ' ' || grid->background_colors[i] != GRID_COLOR_BACKGROUND_DEFAULT) {
            line_length = x + 1;
            break;
        }
    }

    return line_length;
}

// Get the height of the grid without trailing blank lines.
size_t grid_get_occupied_height(struct Grid *grid) {
    size_t occupied_grid_height = 0;
    for (int32_t y = grid->height - 1; y >= 0; y--) {
        if (grid_get_occupied_line_length(grid, y) != 0) {
            occupied_grid_height = y + 1;
            break;
        }
    }

    return occupied_grid_height;
}

void grid_push_partial_line_to_scrollback(struct Grid *grid, size_t y, size_t length) {
    struct ScrollbackLine scrollback_line = {
        .length = length,
    };

    size_t start_offset = y * grid->width;

    scrollback_line.data = malloc(length * sizeof(uint32_t));
    assert(scrollback_line.data);
    memcpy(scrollback_line.data, grid->data + start_offset, length * sizeof(uint32_t));

    scrollback_line.background_colors = malloc(length * sizeof(uint32_t));
    assert(scrollback_line.background_colors);
    memcpy(scrollback_line.background_colors, grid->background_colors + start_offset, length * sizeof(uint32_t));

    scrollback_line.foreground_colors = malloc(length * sizeof(uint32_t));
    assert(scrollback_line.foreground_colors);
    memcpy(scrollback_line.foreground_colors, grid->foreground_colors + start_offset, length * sizeof(uint32_t));

    list_push_struct_ScrollbackLine(&grid->scrollback_lines, scrollback_line);
}

void grid_push_line_to_scrollback(struct Grid *grid, size_t y) {
    size_t length = grid_get_occupied_line_length(grid, y);
    grid_push_partial_line_to_scrollback(grid, y, length);
}

// When resizing the grid, the pseudo console will wrap lines, overwriting
// the lines currently on the screen. It will skip lines that won't fit on the screen, so
// this function pushes those excess lines into the scrollback buffer manually.
void grid_resize_update_scrollback(struct Grid *grid, size_t width, size_t height) {
    int32_t wrapped_line_count = 0;
    size_t occupied_grid_height = grid_get_occupied_height(grid);
    for (size_t y = 0; y < occupied_grid_height; y++) {
        size_t occupied_line_length = grid_get_occupied_line_length(grid, y);
        wrapped_line_count += int32_max((int32_t)ceilf(occupied_line_length / (float)width), 1);
    }

    int32_t excess_line_count = wrapped_line_count - height;
    for (int32_t y = 0; excess_line_count > 0; y++) {
        int32_t occupied_line_length = grid_get_occupied_line_length(grid, y);
        int32_t line_wrapped_line_count = int32_max((int32_t)ceilf(occupied_line_length / (float)width), 1);

        // The whole line is excess.
        if (excess_line_count >= line_wrapped_line_count) {
            grid_push_line_to_scrollback(grid, y);
            excess_line_count -= line_wrapped_line_count;
            continue;
        }

        // Only part of the line is excess (which means this is also the last of the excess lines).
        grid_push_partial_line_to_scrollback(grid, y, excess_line_count * width);
        break;
    }
}

void grid_resize(struct Grid *grid, size_t width, size_t height) {
    grid_resize_update_scrollback(grid, width, height);

    grid->size = width * height;

    size_t old_width = grid->width;
    grid->width = width;
    size_t old_height = grid->height;
    grid->height = height;

    uint32_t *old_data = grid->data;
    grid->data = malloc(grid->size * sizeof(uint32_t));
    assert(grid->data);

    uint32_t *old_background_colors = grid->background_colors;
    grid->background_colors = malloc(grid->size * sizeof(uint32_t));
    assert(grid->background_colors);

    uint32_t *old_foreground_colors = grid->foreground_colors;
    grid->foreground_colors = malloc(grid->size * sizeof(uint32_t));
    assert(grid->foreground_colors);

    for (size_t y = 0; y < grid->height; y++) {
        grid->on_row_changed(grid->callback_context, y);

        for (size_t x = 0; x < grid->width; x++) {
            size_t i = x + y * grid->width;

            if (x < old_width && y < old_height) {
                size_t old_i = x + y * old_width;
                grid->data[i] = old_data[old_i];
                grid->background_colors[i] = old_background_colors[old_i];
                grid->foreground_colors[i] = old_foreground_colors[old_i];

                continue;
            }

            grid->data[i] = ' ';
            grid->background_colors[i] = GRID_COLOR_BACKGROUND_DEFAULT;
            grid->foreground_colors[i] = GRID_COLOR_FOREGROUND_DEFAULT;
        }
    }

    free(old_data);
    free(old_background_colors);
    free(old_foreground_colors);
}

void grid_set_char(struct Grid *grid, int32_t x, int32_t y, uint32_t character) {
    if (x < 0 || y < 0 || x >= grid->width || y >= grid->height) {
        return;
    }

    size_t i = x + y * grid->width;
    grid_set_char_i(grid, i, character);
}

void grid_set_cursor_style(struct Grid *grid, enum GridCursorStyle cursor_style) {
    grid->cursor_style = cursor_style;
    grid->on_row_changed(grid->callback_context, grid->cursor_y);
}

void grid_scroll_down(struct Grid *grid) {
    // Save the first row into the scrollback buffer.
    grid_push_line_to_scrollback(grid, 0);

    // Shift all rows after the first up by one (overwriting the first row).
    size_t preserved_tile_count = grid->size - grid->width;
    memmove(grid->data, grid->data + grid->width, preserved_tile_count * sizeof(uint32_t));
    memmove(grid->background_colors, grid->background_colors + grid->width, preserved_tile_count * sizeof(uint32_t));
    memmove(grid->foreground_colors, grid->foreground_colors + grid->width, preserved_tile_count * sizeof(uint32_t));

    // The cursor didn't move with the rows, so the row the cursor used to be on
    // needs to be updated. The cursor isn't on it anymore.
    if (grid->cursor_y - 1 > 0) {
        grid->on_row_changed(grid->callback_context, grid->cursor_y - 1);
    }

    for (size_t i = 0; i < grid->width; i++) {
        grid_set_char(grid, i, grid->height - 1, ' ');
    }
}

void grid_cursor_save(struct Grid *grid) {
    grid->saved_cursor_x = grid->cursor_x;
    grid->saved_cursor_y = grid->cursor_y;
}

void grid_cursor_restore(struct Grid *grid) {
    grid_cursor_move_to(grid, grid->saved_cursor_x, grid->saved_cursor_y);
}

void grid_cursor_move_to(struct Grid *grid, int32_t x, int32_t y) {
    grid->on_row_changed(grid->callback_context, grid->cursor_y);

    grid->cursor_x = x;
    grid->cursor_y = y;

    if (grid->cursor_x < 0) {
        grid->cursor_x = 0;
    }

    if (grid->cursor_x >= grid->width) {
        grid->cursor_x = grid->width - 1;
    }

    if (grid->cursor_y >= grid->height) {
        grid->cursor_y = grid->height - 1;
    }

    grid->on_row_changed(grid->callback_context, grid->cursor_y);
}

void grid_cursor_move(struct Grid *grid, int32_t delta_x, int32_t delta_y) {
    grid_cursor_move_to(grid, grid->cursor_x + delta_x, grid->cursor_y + delta_y);
}

void grid_swap_current_colors(struct Grid *grid) {
    uint32_t old_background_color = grid->current_background_color;
    grid->current_background_color = grid->current_foreground_color;
    grid->current_foreground_color = old_background_color;
}

void grid_reset_formatting(struct Grid *grid) {
    grid->are_colors_swapped = false;
    grid->current_background_color = GRID_COLOR_BACKGROUND_DEFAULT;
    grid->current_foreground_color = GRID_COLOR_FOREGROUND_DEFAULT;
}

#define PARSE_FAILED                                                                                                   \
    *furthest_i = *i;                                                                                                  \
    *i = start_i;                                                                                                      \
    return false;

void grid_update_mode(struct Grid *grid, int mode, bool enabled) {
    switch (mode) {
        case 25: {
            grid->should_show_cursor = enabled;
            break;
        }
        case 1000: {
            grid->has_mouse_mode_button = enabled;
            break;
        }
        case 1002: {
            grid->has_mouse_mode_drag = enabled;
            break;
        }
        case 1003: {
            grid->has_mouse_mode_any = enabled;
            break;
        }
        case 1006: {
            grid->should_use_sgr_format = enabled;
            break;
        }
    }
}

enum GridMouseMode grid_get_mouse_mode(struct Grid *grid) {
    if (grid->has_mouse_mode_any) {
        return GRID_MOUSE_MODE_ANY;
    }

    if (grid->has_mouse_mode_drag) {
        return GRID_MOUSE_MODE_DRAG;
    }

    if (grid->has_mouse_mode_button) {
        return GRID_MOUSE_MODE_BUTTON;
    }

    return GRID_MOUSE_MODE_NONE;
}

static bool grid_parse_operating_system_command(
    struct Grid *grid,
    struct TextBuffer *text_buffer,
    struct TitleBuffer *title_buffer,
    size_t start_i,
    size_t *i,
    size_t *furthest_i
) {

    if (*i >= text_buffer->length) {
        PARSE_FAILED
    }

    char command_type = text_buffer->data[*i];
    *i += 1;

    if (!text_buffer_match_char(text_buffer, ';', i)) {
        PARSE_FAILED
    }

    // Commands (ie: window titles) can be at most 255 characters.
    for (size_t command_length = 0; command_length < 255; command_length++) {
        size_t peek_i = *i + command_length;
        bool has_bel = text_buffer_match_char(text_buffer, '\x7', &peek_i);
        bool has_terminator = has_bel || (text_buffer_match_char(text_buffer, '\x1b', &peek_i) &&
                                          text_buffer_match_char(text_buffer, '\\', &peek_i));
        if (!has_terminator) {
            continue;
        }

        switch (command_type) {
            // Set window title.
            case '0':
            case '2': {
                if (title_buffer) {
                    memcpy(title_buffer->data, text_buffer->data + *i, command_length);
                    title_buffer->data[command_length] = '\0';
                    title_buffer->is_dirty = true;
                }

                break;
            }
        }

        *i = peek_i;
        return true;
    }

    PARSE_FAILED
}

// Handles parsing cursor visibility and mouse mode.
static bool grid_parse_question_mark(
    struct Grid *grid,
    struct TextBuffer *text_buffer,
    uint32_t parsed_numbers[16],
    size_t parsed_number_count,
    size_t start_i,
    size_t *i,
    size_t *furthest_i
) {

    // Unrecognized numbers here are just ignored, since they are sometimes
    // sent by programs trying to change the mouse mode or other things that we don't support.

    if (text_buffer_match_char(text_buffer, 'h', i)) {
        for (size_t i = 0; i < parsed_number_count; i++) {
            grid_update_mode(grid, parsed_numbers[i], true);
        }

        return true;
    }

    if (text_buffer_match_char(text_buffer, 'l', i)) {
        for (size_t i = 0; i < parsed_number_count; i++) {
            grid_update_mode(grid, parsed_numbers[i], false);
        }

        return true;
    }

    if (text_buffer_match_char(text_buffer, 'u', i)) {
        return true;
    }

    PARSE_FAILED
}

static bool grid_parse_text_formatting(
    struct Grid *grid,
    struct TextBuffer *text_buffer,
    uint32_t parsed_numbers[16],
    size_t parsed_number_count,
    size_t start_i,
    size_t *i,
    size_t *furthest_i
) {

    uint32_t *background_color = &grid->current_background_color;
    uint32_t *foreground_color = &grid->current_foreground_color;

    if (grid->are_colors_swapped) {
        background_color = &grid->current_foreground_color;
        foreground_color = &grid->current_background_color;
    }

    if (parsed_number_count == 0) {
        grid_reset_formatting(grid);
    } else {
        for (size_t i = 0; i < parsed_number_count; i++) {
            switch (parsed_numbers[i]) {
                case 0: {
                    grid_reset_formatting(grid);
                    break;
                }
                case 7: {
                    if (!grid->are_colors_swapped) {
                        grid_swap_current_colors(grid);
                        grid->are_colors_swapped = true;
                    }
                    break;
                }
                case 27: {
                    if (grid->are_colors_swapped) {
                        grid_swap_current_colors(grid);
                        grid->are_colors_swapped = false;
                    }
                    break;
                }
                case 30: {
                    *foreground_color = GRID_COLOR_BLACK;
                    break;
                }
                case 31: {
                    *foreground_color = GRID_COLOR_RED;
                    break;
                }
                case 32: {
                    *foreground_color = GRID_COLOR_GREEN;
                    break;
                }
                case 33: {
                    *foreground_color = GRID_COLOR_YELLOW;
                    break;
                }
                case 34: {
                    *foreground_color = GRID_COLOR_BLUE;
                    break;
                }
                case 35: {
                    *foreground_color = GRID_COLOR_MAGENTA;
                    break;
                }
                case 36: {
                    *foreground_color = GRID_COLOR_CYAN;
                    break;
                }
                case 37: {
                    *foreground_color = GRID_COLOR_WHITE;
                    break;
                }
                case 38: {
                    if (i + 2 < parsed_number_count && parsed_numbers[i + 1] == 5) {
                        size_t color_table_i = parsed_numbers[i + 2] % 256;
                        *foreground_color = color_table[color_table_i];
                        i += 2;
                        break;
                    }

                    if (i + 4 >= parsed_number_count || parsed_numbers[i + 1] != 2) {
                        break;
                    }

                    uint32_t r = parsed_numbers[i + 2];
                    uint32_t g = parsed_numbers[i + 3];
                    uint32_t b = parsed_numbers[i + 4];
                    *foreground_color = (r << 16) | (g << 8) | b;
                    i += 4;
                    break;
                }
                case 39: {
                    *foreground_color = GRID_COLOR_FOREGROUND_DEFAULT;
                    break;
                }
                case 40: {
                    *background_color = GRID_COLOR_BLACK;
                    break;
                }
                case 41: {
                    *background_color = GRID_COLOR_RED;
                    break;
                }
                case 42: {
                    *background_color = GRID_COLOR_GREEN;
                    break;
                }
                case 43: {
                    *background_color = GRID_COLOR_YELLOW;
                    break;
                }
                case 44: {
                    *background_color = GRID_COLOR_BLUE;
                    break;
                }
                case 45: {
                    *background_color = GRID_COLOR_MAGENTA;
                    break;
                }
                case 46: {
                    *background_color = GRID_COLOR_CYAN;
                    break;
                }
                case 47: {
                    *background_color = GRID_COLOR_WHITE;
                    break;
                }
                case 48: {
                    if (i + 2 < parsed_number_count && parsed_numbers[i + 1] == 5) {
                        size_t color_table_i = parsed_numbers[i + 2] % 256;
                        *background_color = color_table[color_table_i];
                        i += 2;
                        break;
                    }

                    if (i + 4 >= parsed_number_count || parsed_numbers[i + 1] != 2) {
                        break;
                    }

                    uint32_t r = parsed_numbers[i + 2];
                    uint32_t g = parsed_numbers[i + 3];
                    uint32_t b = parsed_numbers[i + 4];
                    *background_color = (r << 16) | (g << 8) | b;
                    i += 4;
                    break;
                }
                case 49: {
                    *background_color = GRID_COLOR_BACKGROUND_DEFAULT;
                    break;
                }
                case 90: {
                    *foreground_color = GRID_COLOR_BRIGHT_BLACK;
                    break;
                }
                case 91: {
                    *foreground_color = GRID_COLOR_BRIGHT_RED;
                    break;
                }
                case 92: {
                    *foreground_color = GRID_COLOR_BRIGHT_GREEN;
                    break;
                }
                case 93: {
                    *foreground_color = GRID_COLOR_BRIGHT_YELLOW;
                    break;
                }
                case 94: {
                    *foreground_color = GRID_COLOR_BRIGHT_BLUE;
                    break;
                }
                case 95: {
                    *foreground_color = GRID_COLOR_BRIGHT_MAGENTA;
                    break;
                }
                case 96: {
                    *foreground_color = GRID_COLOR_BRIGHT_CYAN;
                    break;
                }
                case 97: {
                    *foreground_color = GRID_COLOR_BRIGHT_WHITE;
                    break;
                }
                case 100: {
                    *background_color = GRID_COLOR_BRIGHT_BLACK;
                    break;
                }
                case 101: {
                    *background_color = GRID_COLOR_BRIGHT_RED;
                    break;
                }
                case 102: {
                    *background_color = GRID_COLOR_BRIGHT_GREEN;
                    break;
                }
                case 103: {
                    *background_color = GRID_COLOR_BRIGHT_YELLOW;
                    break;
                }
                case 104: {
                    *background_color = GRID_COLOR_BRIGHT_BLUE;
                    break;
                }
                case 105: {
                    *background_color = GRID_COLOR_BRIGHT_MAGENTA;
                    break;
                }
                case 106: {
                    *background_color = GRID_COLOR_BRIGHT_CYAN;
                    break;
                }
                case 107: {
                    *background_color = GRID_COLOR_BRIGHT_WHITE;
                    break;
                }
            }
        }
    }

    return true;
}

static bool grid_parse_cursor_style(
    struct Grid *grid,
    struct TextBuffer *text_buffer,
    uint32_t parsed_numbers[16],
    size_t parsed_number_count,
    size_t start_i,
    size_t *i,
    size_t *furthest_i
) {

    if (text_buffer_match_char(text_buffer, 'q', i) && parsed_number_count == 1) {
        switch (parsed_numbers[0]) {
            case 0:
            case 1:
            case 2: {
                grid_set_cursor_style(grid, GRID_CURSOR_STYLE_BLOCK);
                break;
            }
            case 3:
            case 4: {
                grid_set_cursor_style(grid, GRID_CURSOR_STYLE_UNDERLINE);
                break;
            }
            case 5:
            case 6: {
                grid_set_cursor_style(grid, GRID_CURSOR_STYLE_BAR);
                break;
            }
        }

        return true;
    }

    PARSE_FAILED
}

static bool grid_parse_cursor_positioning(
    struct Grid *grid,
    struct TextBuffer *text_buffer,
    uint32_t parsed_numbers[16],
    size_t parsed_number_count,
    size_t *i
) {

    if (parsed_number_count == 0) {
        if (text_buffer_match_char(text_buffer, 's', i)) {
            grid_cursor_save(grid);
            return true;
        }

        if (text_buffer_match_char(text_buffer, 'u', i)) {
            grid_cursor_restore(grid);
            return true;
        }
    }

    if (parsed_number_count < 2) {
        uint32_t n = parsed_number_count > 0 ? parsed_numbers[0] : 1;

        // Up:
        if (text_buffer_match_char(text_buffer, 'A', i)) {
            grid_cursor_move(grid, 0, -n);
            return true;
        }

        // Down:
        if (text_buffer_match_char(text_buffer, 'B', i)) {
            grid_cursor_move(grid, 0, n);
            return true;
        }

        // Backward:
        if (text_buffer_match_char(text_buffer, 'D', i)) {
            grid_cursor_move(grid, -n, 0);
            return true;
        }

        // Forward:
        if (text_buffer_match_char(text_buffer, 'C', i)) {
            grid_cursor_move(grid, n, 0);
            return true;
        }

        // Horizontal absolute:
        if (text_buffer_match_char(text_buffer, 'G', i)) {
            if (n > 0) {
                grid_cursor_move_to(grid, n - 1, grid->cursor_y);
            }
            return true;
        }

        // Vertical absolute:
        if (text_buffer_match_char(text_buffer, 'd', i)) {
            if (n > 0) {
                grid_cursor_move_to(grid, grid->cursor_x, n - 1);
            }
            return true;
        }
    }

    uint32_t y = parsed_number_count > 0 ? parsed_numbers[0] : 1;
    uint32_t x = parsed_number_count > 1 ? parsed_numbers[1] : 1;

    // Cursor position or horizontal vertical position:
    if (text_buffer_match_char(text_buffer, 'H', i) || text_buffer_match_char(text_buffer, 'f', i)) {
        if (x > 0 && y > 0) {
            grid_cursor_move_to(grid, x - 1, y - 1);
        }
        return true;
    }

    return false;
}

static bool grid_parse_text_modification(
    struct Grid *grid,
    struct TextBuffer *text_buffer,
    uint32_t parsed_numbers[16],
    size_t parsed_number_count,
    size_t *i
) {

    uint32_t n = parsed_numbers[0];

    if (text_buffer_match_char(text_buffer, 'X', i)) {
        uint32_t erase_start = grid->cursor_x + grid->cursor_y * grid->width;
        uint32_t erase_count = grid->size - erase_start;
        if (n < erase_count) {
            erase_count = n;
        }

        for (size_t erase_i = erase_start; erase_i < erase_start + erase_count; erase_i++) {
            grid_set_char_i(grid, erase_i, ' ');
        }

        return true;
    }

    switch (n) {
        case 0: {
            // Erase display after cursor.
            if (text_buffer_match_char(text_buffer, 'J', i)) {
                uint32_t erase_start = grid->cursor_x + grid->cursor_y * grid->width;
                uint32_t erase_count = grid->size - erase_start;

                for (size_t erase_i = erase_start; erase_i < erase_start + erase_count; erase_i++) {
                    grid_set_char_i(grid, erase_i, ' ');
                }

                return true;
            }
            // Erase line after cursor.
            else if (text_buffer_match_char(text_buffer, 'K', i)) {
                for (size_t x = grid->cursor_x; x < grid->width; x++) {
                    grid_set_char(grid, x, grid->cursor_y, ' ');
                }

                return true;
            }

            break;
        }
        case 1: {
            // Erase display before cursor.
            if (text_buffer_match_char(text_buffer, 'J', i)) {
                uint32_t erase_count = grid->cursor_x + grid->cursor_y * grid->width;

                for (size_t erase_i = 0; erase_i <= erase_count; erase_i++) {
                    grid_set_char_i(grid, erase_i, ' ');
                }

                return true;
            }
            // Erase line before cursor.
            else if (text_buffer_match_char(text_buffer, 'K', i)) {
                for (size_t x = 0; x <= grid->cursor_x; x++) {
                    grid_set_char(grid, x, grid->cursor_y, ' ');
                }

                return true;
            }

            break;
        }
        case 2: {
            // Erase entire display.
            if (text_buffer_match_char(text_buffer, 'J', i)) {
                for (size_t y = 0; y < grid->height; y++) {
                    for (size_t x = 0; x < grid->width; x++) {
                        grid_set_char(grid, x, y, ' ');
                    }
                }

                return true;
            }
            // Erase entire line.
            else if (text_buffer_match_char(text_buffer, 'K', i)) {
                for (size_t x = 0; x < grid->width; x++) {
                    grid_set_char(grid, x, grid->cursor_y, ' ');
                }

                return true;
            }

            break;
        }
    }

    return false;
}

// Returns true if an escape sequence was parsed.
bool grid_parse_escape_sequence(
    struct Grid *grid, struct TextBuffer *text_buffer, struct TitleBuffer *title_buffer, size_t *i, size_t *furthest_i
) {

    size_t start_i = *i;
    if (!text_buffer_match_char(text_buffer, '\x1b', i)) {
        PARSE_FAILED
    }

    // Simple cursor positioning:
    if (text_buffer_match_char(text_buffer, '7', i)) {
        grid_cursor_save(grid);
        return true;
    }

    if (text_buffer_match_char(text_buffer, '8', i)) {
        grid_cursor_restore(grid);
        return true;
    }

    if (text_buffer_match_char(text_buffer, 'H', i)) {
        // TODO:
        puts("set tab stop");
        return true;
    }

    // Operating system commands:
    if (text_buffer_match_char(text_buffer, ']', i)) {
        return grid_parse_operating_system_command(grid, text_buffer, title_buffer, start_i, i, furthest_i);
    }

    // Control sequence introducers:
    if (!text_buffer_match_char(text_buffer, '[', i)) {
        PARSE_FAILED
    }

    bool is_unsupported = text_buffer_match_char(text_buffer, '>', i);
    bool starts_with_question_mark = text_buffer_match_char(text_buffer, '?', i);

    // The maximum amount of numbers supported is 16, for the "m" commands (text formatting).
    uint32_t parsed_numbers[16] = {0};
    size_t parsed_number_count = 0;

    for (size_t parsed_number_i = 0; parsed_number_i < 16; parsed_number_i++) {
        bool did_parse_number = false;
        while (text_buffer_digit(text_buffer, *i)) {
            did_parse_number = true;
            uint32_t digit = text_buffer->data[*i] - '0';
            parsed_numbers[parsed_number_i] = parsed_numbers[parsed_number_i] * 10 + digit;
            *i += 1;
        }

        if (did_parse_number) {
            parsed_number_count++;
        } else {
            break;
        }

        if (!text_buffer_match_char(text_buffer, ';', i)) {
            break;
        }
    }

    // Formats like ESC[>[numbers][character] are not supported.
    if (is_unsupported) {
        *i += 1;
        return true;
    }

    if (starts_with_question_mark) {
        return grid_parse_question_mark(grid, text_buffer, parsed_numbers, parsed_number_count, start_i, i, furthest_i);
    }

    if (text_buffer_match_char(text_buffer, ' ', i)) {
        return grid_parse_cursor_style(grid, text_buffer, parsed_numbers, parsed_number_count, start_i, i, furthest_i);
    }

    if (text_buffer_match_char(text_buffer, 'm', i)) {
        return grid_parse_text_formatting(
            grid,
            text_buffer,
            parsed_numbers,
            parsed_number_count,
            start_i,
            i,
            furthest_i
        );
    }

    if (grid_parse_cursor_positioning(grid, text_buffer, parsed_numbers, parsed_number_count, i)) {
        return true;
    }

    if (grid_parse_text_modification(grid, text_buffer, parsed_numbers, parsed_number_count, i)) {
        return true;
    }

    // This sequence is invalid, ignore it.
    PARSE_FAILED
}

void grid_destroy(struct Grid *grid) {
    free(grid->data);

    for (size_t i = 0; i < grid->scrollback_lines.length; i++) {
        free(grid->scrollback_lines.data[i].data);
        free(grid->scrollback_lines.data[i].background_colors);
        free(grid->scrollback_lines.data[i].foreground_colors);
    }
    list_destroy_struct_ScrollbackLine(&grid->scrollback_lines);

    free(grid->background_colors);
    free(grid->foreground_colors);
}

extern inline void grid_set_char_i(struct Grid *grid, int32_t i, uint32_t character);