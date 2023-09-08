#include "grid.h"

#include "color.h"
#include "font.h"

#define GRID_BACKGROUND_COLOR_DEFAULT 0x000000
#define GRID_FOREGROUND_COLOR_DEFAULT 0xffffff

struct Grid grid_create(size_t width, size_t height) {
    size_t size = width * height;
    struct Grid grid = (struct Grid){
        .data = malloc(size * sizeof(char)),
        .are_rows_dirty = malloc(height * sizeof(bool)),
        .width = width,
        .height = height,
        .size = size,

        .background_colors = malloc(size * sizeof(uint32_t)),
        .foreground_colors = malloc(size * sizeof(uint32_t)),

        .current_background_color = GRID_BACKGROUND_COLOR_DEFAULT,
        .current_foreground_color = GRID_FOREGROUND_COLOR_DEFAULT,
    };
    assert(grid.data);
    assert(grid.are_rows_dirty);

    for (size_t i = 0; i < size; i++) {
        grid_set_char_i(&grid, i, ' ');
    }

    return grid;
}

void grid_resize(struct Grid *grid, size_t width, size_t height) {
    grid->size = width * height;

    size_t old_width = grid->width;
    grid->width = width;
    size_t old_height = grid->height;
    grid->height = height;

    char *old_data = grid->data;
    grid->data = malloc(grid->size * sizeof(char));
    assert(grid->data);

    uint32_t *old_background_colors = grid->background_colors;
    grid->background_colors = malloc(grid->size * sizeof(uint32_t));
    assert(grid->background_colors);

    uint32_t *old_foreground_colors = grid->foreground_colors;
    grid->foreground_colors = malloc(grid->size * sizeof(uint32_t));
    assert(grid->foreground_colors);

    free(grid->are_rows_dirty);
    grid->are_rows_dirty = malloc(grid->height * sizeof(bool));
    assert(grid->are_rows_dirty);

    for (size_t y = 0; y < grid->height; y++) {
        grid->are_rows_dirty[y] = true;

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
            grid->background_colors[i] = GRID_BACKGROUND_COLOR_DEFAULT;
            grid->foreground_colors[i] = GRID_FOREGROUND_COLOR_DEFAULT;
        }
    }

    free(old_data);
    free(old_background_colors);
    free(old_foreground_colors);
}

void grid_set_char(struct Grid *grid, int32_t x, int32_t y, char character) {
    if (x < 0 || y < 0 || x >= grid->width || y >= grid->height) {
        return;
    }

    size_t i = x + y * grid->width;
    grid_set_char_i(grid, i, character);
}

void grid_scroll_down(struct Grid *grid) {
    grid->are_rows_dirty[grid->cursor_y] = true;

    memmove(&grid->data[0], &grid->data[grid->width], (grid->size - grid->width) * sizeof(char));
    memmove(&grid->are_rows_dirty[0], &grid->are_rows_dirty[1], (grid->height - 1) * sizeof(bool));

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
    grid->are_rows_dirty[grid->cursor_y] = true;

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

    grid->are_rows_dirty[grid->cursor_y] = true;
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
    grid->current_background_color = GRID_BACKGROUND_COLOR_DEFAULT;
    grid->current_foreground_color = GRID_FOREGROUND_COLOR_DEFAULT;
}

// Returns true if an escape sequence was parsed.
bool grid_parse_escape_sequence(struct Grid *grid, struct TextBuffer *data, size_t *i, struct Window *window) {
    size_t start_i = *i;
    if (!text_buffer_match_char(data, '\x1b', i)) {
        *i = start_i;
        return false;
    }

    // Simple cursor positioning:
    if (text_buffer_match_char(data, '7', i)) {
        grid_cursor_save(grid);
        return true;
    }

    if (text_buffer_match_char(data, '8', i)) {
        grid_cursor_restore(grid);
        return true;
    }

    if (text_buffer_match_char(data, 'H', i)) {
        // TODO:
        puts("set tab stop");
        return true;
    }

    // Operating system commands:
    if (text_buffer_match_char(data, ']', i)) {
        bool has_zero_or_two = text_buffer_match_char(data, '0', i) || text_buffer_match_char(data, '2', i);
        if (!has_zero_or_two || !text_buffer_match_char(data, ';', i)) {
            *i = start_i;
            return false;
        }

        // Window titles can be at most 255 characters.
        for (size_t title_length = 0; title_length < 255; title_length++) {
            size_t peek_i = *i + title_length;
            bool has_bel = text_buffer_match_char(data, '\x7', &peek_i);
            bool has_terminator = has_bel || (text_buffer_match_char(data, '\x1b', &peek_i) &&
                                                 text_buffer_match_char(data, '\x5c', &peek_i));
            if (!has_terminator) {
                continue;
            }

            size_t title_string_length = title_length + 1;
            char *title = malloc(title_string_length);
            memcpy(title, data->data + *i, title_length);
            title[title_length] = '\0';

            glfwSetWindowTitle(window->glfw_window, (char *)title);

            free(title);

            *i = peek_i;
            return true;
        }
    }

    // Control sequence introducers:
    if (text_buffer_match_char(data, '[', i)) {
        bool is_unsupported = text_buffer_match_char(data, '>', i);
        bool starts_with_question_mark = text_buffer_match_char(data, '?', i);

        // TODO: Make sure when parsing sequences that they don't have more numbers supplied then they allow, ie: no
        // ESC[10;5A because A should only accept one number.
        // The maximum amount of numbers supported is 16, for the "m" commands (text formatting).
        uint32_t parsed_numbers[16] = {0};
        size_t parsed_number_count = 0;

        for (size_t parsed_number_i = 0; parsed_number_i < 16; parsed_number_i++) {
            bool did_parse_number = false;
            while (text_buffer_digit(data, *i)) {
                did_parse_number = true;
                uint32_t digit = data->data[*i] - '0';
                parsed_numbers[parsed_number_i] = parsed_numbers[parsed_number_i] * 10 + digit;
                *i += 1;
            }

            if (did_parse_number) {
                parsed_number_count++;
            } else {
                break;
            }

            if (!text_buffer_match_char(data, ';', i)) {
                break;
            }
        }

        // Formats like ESC[>[numbers][character] are not supported.
        if (is_unsupported) {
            *i += 1;
            return true;
        }

        // Cursor visibility:
        if (starts_with_question_mark) {
            // Unrecognized numbers here are just ignored, since they are sometimes
            // sent by programs trying to change the mouse mode or other things that we don't support.
            bool should_show_hide_cursor = parsed_number_count == 1 && parsed_numbers[0] == 25;

            if (text_buffer_match_char(data, 'h', i)) {
                if (should_show_hide_cursor) {
                    grid->show_cursor = true;
                }
                return true;
            }

            if (text_buffer_match_char(data, 'l', i)) {
                if (should_show_hide_cursor) {
                    grid->show_cursor = false;
                }
                return true;
            }

            if (text_buffer_match_char(data, 'u', i)) {
                return true;
            }

            *i = start_i;
            return false;
        }

        // Cursor shape:
        if (text_buffer_match_char(data, ' ', i)) {
            if (text_buffer_match_char(data, 'q', i)) {
                // TODO: Change cursor shape.
                return true;
            }

            *i = start_i;
            return false;
        }

        // Text formatting:
        {
            if (text_buffer_match_char(data, 'm', i)) {
                // TODO: Support more formats.

                uint32_t *background_color = &grid->current_background_color;
                uint32_t *foreground_color = &grid->current_foreground_color;

                if (grid->are_colors_swapped) {
                    background_color = &grid->current_foreground_color;
                    foreground_color = &grid->current_background_color;
                }

                if (parsed_number_count == 0) {
                    grid_reset_formatting(grid);
                }
                // Set foreground RGB.
                else if (parsed_numbers[0] == 38 && parsed_numbers[1] == 2) {
                    uint32_t r = parsed_numbers[2];
                    uint32_t g = parsed_numbers[3];
                    uint32_t b = parsed_numbers[4];
                    *foreground_color = (r << 16) | (g << 8) | b;
                }
                // Set background RGB.
                else if (parsed_numbers[0] == 48 && parsed_numbers[1] == 2) {
                    uint32_t r = parsed_numbers[2];
                    uint32_t g = parsed_numbers[3];
                    uint32_t b = parsed_numbers[4];
                    *background_color = (r << 16) | (g << 8) | b;
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
                            case 30:
                            case 31:
                            case 32:
                            case 33:
                            case 34:
                            case 35:
                            case 36:
                            case 37:
                            case 38:
                            case 39: {
                                *foreground_color = GRID_FOREGROUND_COLOR_DEFAULT;
                                break;
                            }
                            case 40:
                            case 41:
                            case 42:
                            case 43:
                            case 44:
                            case 45:
                            case 46:
                            case 47:
                            case 48:
                            case 49: {
                                *background_color = GRID_BACKGROUND_COLOR_DEFAULT;
                                break;
                            }
                        }
                    }
                }

                return true;
            }
        }

        // Cursor positioning:
        {
            if (parsed_number_count == 0) {
                if (text_buffer_match_char(data, 's', i)) {
                    grid_cursor_save(grid);
                    return true;
                }

                if (text_buffer_match_char(data, 'u', i)) {
                    grid_cursor_restore(grid);
                    return true;
                }
            }

            if (parsed_number_count < 2) {
                uint32_t n = parsed_number_count > 0 ? parsed_numbers[0] : 1;

                // Up:
                if (text_buffer_match_char(data, 'A', i)) {
                    grid_cursor_move(grid, 0, -n);
                    return true;
                }

                // Down:
                if (text_buffer_match_char(data, 'B', i)) {
                    grid_cursor_move(grid, 0, n);
                    return true;
                }

                // Backward:
                if (text_buffer_match_char(data, 'D', i)) {
                    grid_cursor_move(grid, -n, 0);
                    return true;
                }

                // Forward:
                if (text_buffer_match_char(data, 'C', i)) {
                    grid_cursor_move(grid, n, 0);
                    return true;
                }

                // Horizontal absolute:
                if (text_buffer_match_char(data, 'G', i)) {
                    if (n > 0) {
                        grid_cursor_move_to(grid, n - 1, grid->cursor_y);
                    }
                    return true;
                }

                // Vertical absolute:
                if (text_buffer_match_char(data, 'd', i)) {
                    if (n > 0) {
                        grid_cursor_move_to(grid, grid->cursor_x, n - 1);
                    }
                    return true;
                }
            }

            uint32_t y = parsed_number_count > 0 ? parsed_numbers[0] : 1;
            uint32_t x = parsed_number_count > 1 ? parsed_numbers[1] : 1;

            // Cursor position or horizontal vertical position:
            if (text_buffer_match_char(data, 'H', i) || text_buffer_match_char(data, 'f', i)) {
                if (x > 0 && y > 0) {
                    grid_cursor_move_to(grid, x - 1, y - 1);
                }
                return true;
            }
        }

        // Text modification:
        {
            uint32_t n = parsed_number_count > 0 ? parsed_numbers[0] : 0;

            if (text_buffer_match_char(data, 'X', i)) {
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
                    // Erase line after cursor.
                    if (text_buffer_match_char(data, 'K', i)) {
                        for (size_t x = grid->cursor_x; x < grid->width; x++) {
                            grid_set_char(grid, x, grid->cursor_y, ' ');
                        }

                        return true;
                    }

                    break;
                }
                case 1: {
                    if (text_buffer_match_char(data, 'J', i)) {
                        // Erase display before cursor.
                        uint32_t erase_start = grid->cursor_x + grid->cursor_y * grid->width;
                        uint32_t erase_count = grid->size - erase_start;

                        for (size_t erase_i = erase_start; erase_i < erase_start + erase_count; erase_i++) {
                            grid_set_char_i(grid, erase_i, ' ');
                        }

                        return true;
                    }

                    break;
                }
                case 2: {
                    if (text_buffer_match_char(data, 'J', i)) {
                        // Erase entire display.
                        for (size_t y = 0; y < grid->height; y++) {
                            for (size_t x = 0; x < grid->width; x++) {
                                grid_set_char(grid, x, y, ' ');
                            }
                        }

                        return true;
                    }
                    // Erase entire line.
                    else if (text_buffer_match_char(data, 'K', i)) {
                        for (size_t x = 0; x < grid->width; x++) {
                            grid_set_char(grid, x, grid->cursor_y, ' ');
                        }

                        return true;
                    }

                    break;
                }
            }
        }
    }

    // This sequence is invalid, ignore it.
    *i = start_i;
    return false;
}

void grid_draw_character(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, float scale, float r, float g, float b) {

    char character = grid->data[x + y * grid->width];
    if (character == ' ') {
        return;
    }

    sprite_batch_add(sprite_batch, (struct Sprite){
                                       .x = x * FONT_GLYPH_WIDTH * scale,
                                       .z = z * scale,
                                       .width = FONT_GLYPH_WIDTH * scale,
                                       .height = FONT_GLYPH_HEIGHT * scale,

                                       .texture_x = 8 * (character - 32),
                                       .texture_width = FONT_GLYPH_WIDTH,
                                       .texture_height = FONT_GLYPH_HEIGHT,

                                       .r = r,
                                       .g = g,
                                       .b = b,
                                   });
}

void grid_draw_box(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t z, float scale, float r, float g, float b) {

    sprite_batch_add(sprite_batch, (struct Sprite){
                                       .x = x * FONT_GLYPH_WIDTH * scale,
                                       .z = z * scale,
                                       .width = FONT_GLYPH_WIDTH * scale,
                                       .height = FONT_GLYPH_HEIGHT * scale,

                                       .texture_x = 0,
                                       .texture_width = FONT_GLYPH_WIDTH,
                                       .texture_height = FONT_GLYPH_HEIGHT,

                                       .r = r,
                                       .g = g,
                                       .b = b,
                                   });
}

void grid_draw_tile(struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, float scale) {

    size_t i = x + y * grid->width;
    struct Color background_color = color_from_hex(grid->background_colors[i]);
    grid_draw_box(grid, sprite_batch, x, z, scale, background_color.r, background_color.g, background_color.b);
    struct Color foreground_color = color_from_hex(grid->foreground_colors[i]);
    grid_draw_character(grid, sprite_batch, x, y, z + 1, scale, foreground_color.r, foreground_color.g, foreground_color.b);
}

void grid_draw_cursor(struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, float scale) {
    grid_draw_box(grid, sprite_batch, x, z, scale, 1.0f, 1.0f, 1.0f);
    grid_draw_character(grid, sprite_batch, x, y, z + 1, scale, 0.0f, 0.0f, 0.0f);
}

void grid_destroy(struct Grid *grid) {
    free(grid->data);
    free(grid->are_rows_dirty);
    free(grid->background_colors);
    free(grid->foreground_colors);
}

extern inline void grid_set_char_i(struct Grid *grid, int32_t i, char character);