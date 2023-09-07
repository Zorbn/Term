#include "detect_leak.h"

#include "window.h"
#include "graphics/resources.h"
#include "graphics/sprite_batch.h"

#include <cglm/struct.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

/*
 * TODO:
 * Dynamic resizing.
 * Unicode characters and box drawing characters aren't handled well,
 * (ie: Helix breaks when it shows a unicode animation after opening a file, or when it does box drawing while typing a
 * command).
 */

/*
 * Missing features:
 * Mouse input,
 * Copy/paste,
 * Scrollback,
 */

// TODO: Is this too big for the buffer? Is there a way to make it smaller while not splitting escape codes and failing
// to process them?
#define READ_BUFFER_SIZE 8192

const float sky_color_r = 50.0f / 255.0f;
const float sky_color_g = 74.0f / 255.0f;
const float sky_color_b = 117.0f / 255.0f;

typedef struct {
    wchar_t *text;
    UINT32 textLength;
    RECT rc;
} Data;

bool data_match_char(Data *data, wchar_t character, size_t *i) {
    if (*i >= data->textLength) {
        return false;
    }

    if (data->text[*i] != character) {
        return false;
    }

    *i += 1;
    return true;
}

bool data_peek_digit(Data *data, size_t i) {
    if (i >= data->textLength) {
        return false;
    }

    return iswdigit(data->text[i]);
}

void data_destroy(Data *data) {
    free(data->text);
}

struct Color {
    float r;
    float g;
    float b;
};

struct Color color_from_hex(uint32_t hex) {
    return (struct Color){
        .r = (hex >> 16) / 255.0f,
        .g = ((hex >> 8) & 0xff) / 255.0f,
        .b = (hex & 0xff) / 255.0f,
    };
}

#define GRID_BACKGROUND_COLOR_DEFAULT 0x000000
#define GRID_FOREGROUND_COLOR_DEFAULT 0xffffff

struct Grid {
    wchar_t *data;
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

inline void grid_set_char_i(struct Grid *grid, int32_t i, wchar_t character) {
    grid->data[i] = character;
    grid->background_colors[i] = grid->current_background_color;
    grid->foreground_colors[i] = grid->current_foreground_color;
}

struct Grid grid_create(size_t width, size_t height) {
    size_t size = width * height;
    struct Grid grid = (struct Grid){
        .data = malloc(size * sizeof(wchar_t)),
        .width = width,
        .height = height,
        .size = size,

        .background_colors = malloc(size * sizeof(uint32_t)),
        .foreground_colors = malloc(size * sizeof(uint32_t)),

        .current_background_color = GRID_BACKGROUND_COLOR_DEFAULT,
        .current_foreground_color = GRID_FOREGROUND_COLOR_DEFAULT,
    };
    assert(grid.data);

    for (size_t i = 0; i < size; i++) {
        grid_set_char_i(&grid, i, L' ');
    }

    return grid;
}

void grid_destroy(struct Grid *grid) {
    free(grid->data);
}

void grid_set_char(struct Grid *grid, int32_t x, int32_t y, wchar_t character) {
    if (x < 0 || y < 0 || x >= grid->width || y >= grid->height) {
        return;
    }

    size_t i = x + y * grid->width;
    grid_set_char_i(grid, i, character);
}

void grid_scroll_down(struct Grid *grid) {
    memmove(&grid->data[0], &grid->data[grid->width], (grid->size - grid->width) * sizeof(wchar_t));

    for (size_t i = 0; i < grid->width; i++) {
        grid_set_char(grid, i, grid->height - 1, L' ');
    }
}

void grid_cursor_save(struct Grid *grid) {
    grid->saved_cursor_x = grid->cursor_x;
    grid->saved_cursor_y = grid->cursor_y;
}

void grid_cursor_restore(struct Grid *grid) {
    grid->cursor_x = grid->saved_cursor_x;
    grid->cursor_y = grid->saved_cursor_y;
}

void grid_cursor_move_to(struct Grid *grid, int32_t x, int32_t y) {
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
bool grid_parse_escape_sequence(struct Grid *grid, Data *data, size_t *i, struct Window *window) {
    size_t start_i = *i;
    if (!data_match_char(data, L'\x1b', i)) {
        *i = start_i;
        return false;
    }

    // Simple cursor positioning:
    if (data_match_char(data, L'7', i)) {
        grid_cursor_save(grid);
        return true;
    }

    if (data_match_char(data, L'8', i)) {
        grid_cursor_restore(grid);
        return true;
    }

    if (data_match_char(data, L'H', i)) {
        // TODO:
        puts("set tab stop");
        return true;
    }

    // Operating system commands:
    if (data_match_char(data, L']', i)) {
        bool has_zero_or_two = data_match_char(data, L'0', i) || data_match_char(data, L'2', i);
        if (!has_zero_or_two || !data_match_char(data, L';', i)) {
            *i = start_i;
            return false;
        }

        // Window titles can be at most 255 characters.
        for (size_t title_length = 0; title_length < 255; title_length++) {
            size_t peek_i = *i + title_length;
            bool has_bel = data_match_char(data, L'\x7', &peek_i);
            bool has_terminator =
                has_bel || (data_match_char(data, L'\x1b', &peek_i) && data_match_char(data, L'\x5c', &peek_i));
            if (!has_terminator) {
                continue;
            }

            size_t title_string_length = title_length + 1;
            char *title = malloc(title_string_length);
            wcstombs_s(NULL, title, title_string_length, data->text + *i, title_length);

            glfwSetWindowTitle(window->glfw_window, (char *)title);

            free(title);

            *i = peek_i;
            return true;
        }
    }

    // Control sequence introducers:
    if (data_match_char(data, L'[', i)) {
        bool is_unsupported = data_match_char(data, L'>', i);
        bool starts_with_question_mark = data_match_char(data, L'?', i);

        // TODO: Make sure when parsing sequences that they don't have more numbers supplied then they allow, ie: no
        // ESC[10;5A because A should only accept one number.
        // The maximum amount of numbers supported is 16, for the "m" commands (text formatting).
        uint32_t parsed_numbers[16] = {0};
        size_t parsed_number_count = 0;

        for (size_t parsed_number_i = 0; parsed_number_i < 16; parsed_number_i++) {
            bool did_parse_number = false;
            while (data_peek_digit(data, *i)) {
                did_parse_number = true;
                uint32_t digit = data->text[*i] - L'0';
                parsed_numbers[parsed_number_i] = parsed_numbers[parsed_number_i] * 10 + digit;
                *i += 1;
            }

            if (did_parse_number) {
                parsed_number_count++;
            } else {
                break;
            }

            if (!data_match_char(data, L';', i)) {
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

            if (data_match_char(data, L'h', i)) {
                if (should_show_hide_cursor) {
                    grid->show_cursor = true;
                }
                return true;
            }

            if (data_match_char(data, L'l', i)) {
                if (should_show_hide_cursor) {
                    grid->show_cursor = false;
                }
                return true;
            }

            if (data_match_char(data, L'u', i)) {
                return true;
            }

            *i = start_i;
            return false;
        }

        // Cursor shape:
        if (data_match_char(data, L' ', i)) {
            if (data_match_char(data, L'q', i)) {
                // TODO: Change cursor shape.
                return true;
            }

            *i = start_i;
            return false;
        }

        // Text formatting:
        {
            if (data_match_char(data, L'm', i)) {
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
                }
                else {
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
                if (data_match_char(data, L's', i)) {
                    grid_cursor_save(grid);
                    return true;
                }

                if (data_match_char(data, L'u', i)) {
                    grid_cursor_restore(grid);
                    return true;
                }
            }

            if (parsed_number_count < 2) {
                uint32_t n = parsed_number_count > 0 ? parsed_numbers[0] : 1;

                // Up:
                if (data_match_char(data, L'A', i)) {
                    grid_cursor_move(grid, 0, -n);
                    return true;
                }

                // Down:
                if (data_match_char(data, L'B', i)) {
                    grid_cursor_move(grid, 0, n);
                    return true;
                }

                // Backward:
                if (data_match_char(data, L'D', i)) {
                    grid_cursor_move(grid, -n, 0);
                    return true;
                }

                // Forward:
                if (data_match_char(data, L'C', i)) {
                    grid_cursor_move(grid, n, 0);
                    return true;
                }

                // Horizontal absolute:
                if (data_match_char(data, L'G', i)) {
                    if (n > 0) {
                        grid_cursor_move_to(grid, n - 1, grid->cursor_y);
                    }
                    return true;
                }

                // Vertical absolute:
                if (data_match_char(data, L'd', i)) {
                    if (n > 0) {
                        grid_cursor_move_to(grid, grid->cursor_x, n - 1);
                    }
                    return true;
                }
            }

            uint32_t y = parsed_number_count > 0 ? parsed_numbers[0] : 1;
            uint32_t x = parsed_number_count > 1 ? parsed_numbers[1] : 1;

            // Cursor position or horizontal vertical position:
            if (data_match_char(data, L'H', i) || data_match_char(data, L'f', i)) {
                if (x > 0 && y > 0) {
                    grid_cursor_move_to(grid, x - 1, y - 1);
                }
                return true;
            }
        }

        // Text modification:
        {
            uint32_t n = parsed_number_count > 0 ? parsed_numbers[0] : 0;

            if (data_match_char(data, L'X', i)) {
                uint32_t erase_start = grid->cursor_x + grid->cursor_y * grid->width;
                uint32_t erase_count = grid->size - erase_start;
                if (n < erase_count) {
                    erase_count = n;
                }

                for (size_t erase_i = erase_start; erase_i < erase_start + erase_count; erase_i++) {
                    grid_set_char_i(grid, erase_i, L' ');
                }

                return true;
            }

            switch (n) {
                case 0: {
                    // Erase line after cursor.
                    if (data_match_char(data, L'K', i)) {
                        for (size_t x = grid->cursor_x; x < grid->width; x++) {
                            grid_set_char(grid, x, grid->cursor_y, L' ');
                        }

                        return true;
                    }

                    break;
                }
                case 1: {
                    if (data_match_char(data, L'J', i)) {
                        // Erase display before cursor.
                        uint32_t erase_start = grid->cursor_x + grid->cursor_y * grid->width;
                        uint32_t erase_count = grid->size - erase_start;

                        for (size_t erase_i = erase_start; erase_i < erase_start + erase_count; erase_i++) {
                            grid_set_char_i(grid, erase_i, L' ');
                        }

                        return true;
                    }

                    break;
                }
                case 2: {
                    if (data_match_char(data, L'J', i)) {
                        // Erase entire display.
                        for (size_t y = 0; y < grid->height; y++) {
                            for (size_t x = 0; x < grid->width; x++) {
                                grid_set_char(grid, x, y, L' ');
                            }
                        }

                        return true;
                    }
                    // Erase entire line.
                    else if (data_match_char(data, L'K', i)) {
                        for (size_t x = 0; x < grid->width; x++) {
                            grid_set_char(grid, x, grid->cursor_y, L' ');
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

void grid_draw_character(struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z,
    int32_t origin_y, float r, float g, float b) {

    wchar_t character = grid->data[x + y * grid->width];
    if (character < 33 || character > 126) {
        return;
    }

    sprite_batch_add(sprite_batch, (struct Sprite){
                                       .x = x * 6,
                                       .y = origin_y - (y + 1) * 14,
                                       .z = z,
                                       .width = 6,
                                       .height = 14,

                                       .texture_x = 8 * (character - 32),
                                       .texture_width = 6,
                                       .texture_height = 14,

                                       .r = r,
                                       .g = g,
                                       .b = b,
                                   });
}

void grid_draw_box(struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z,
    int32_t origin_y, float r, float g, float b) {

    sprite_batch_add(sprite_batch, (struct Sprite){
                                       .x = x * 6,
                                       .y = origin_y - (y + 1) * 14,
                                       .z = z,
                                       .width = 6,
                                       .height = 14,

                                       .texture_x = 0,
                                       .texture_width = 6,
                                       .texture_height = 14,

                                       .r = r,
                                       .g = g,
                                       .b = b,
                                   });
}

void grid_draw_tile(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, int32_t origin_y) {

    size_t i = x + y * grid->width;
    struct Color background_color = color_from_hex(grid->background_colors[i]);
    grid_draw_box(grid, sprite_batch, x, y, z, origin_y, background_color.r, background_color.g, background_color.b);
    struct Color foreground_color = color_from_hex(grid->foreground_colors[i]);
    grid_draw_character(
        grid, sprite_batch, x, y, z + 1, origin_y, foreground_color.r, foreground_color.g, foreground_color.b);
}

void grid_draw_cursor(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, int32_t origin_y) {

    grid_draw_box(grid, sprite_batch, x, y, z, origin_y, 1.0f, 1.0f, 1.0f);
    grid_draw_character(grid, sprite_batch, x, y, z + 1, origin_y, 0.0f, 0.0f, 0.0f);
}

int main() {
    struct Window window = window_create("CBlock", 640, 480);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    uint32_t program_2d = program_create("assets/shader_2d.vert", "assets/shader_2d.frag");

    // TODO: Add texture_bind and texture_destroy
    struct Texture texture_atlas_2d = texture_create("assets/texture_atlas.png");

    struct SpriteBatch sprite_batch = sprite_batch_create(16);

    mat4s projection_matrix_2d;

    int32_t projection_matrix_location_2d = glGetUniformLocation(program_2d, "projection_matrix");

    Data data = {0};
    const size_t data_text_capacity = READ_BUFFER_SIZE + 1;
    data.text = malloc(data_text_capacity * sizeof(wchar_t));
    assert(data.text);
    data.textLength = 0;

    const int32_t grid_width = window.width / 6;
    const int32_t grid_height = window.height / 14;
    window.console = SetUpPseudoConsole((COORD){grid_width, grid_height});
    printf("Console result: %ld\n", window.console.result);
    struct Grid grid = grid_create(grid_width, grid_height);

    CHAR read_buffer[READ_BUFFER_SIZE];

    double last_frame_time = glfwGetTime();
    float fps_print_timer = 0.0f;

    float elapsed_time = 0.0f;

    while (!glfwWindowShouldClose(window.glfw_window)) {
        // Update:
        if (window.was_resized) {
            glViewport(0, 0, window.width, window.height);
            projection_matrix_2d = glms_ortho(0.0f, (float)window.width, 0.0f, (float)window.height, -100.0, 100.0);

            window.was_resized = false;
        }

        double current_frame_time = glfwGetTime();
        float delta_time = (float)(current_frame_time - last_frame_time);
        last_frame_time = current_frame_time;

        fps_print_timer += delta_time;

        if (fps_print_timer > 1.0f) {
            fps_print_timer = 0.0f;
            printf("fps: %f\n", 1.0f / delta_time);
        }

        elapsed_time += delta_time;
        float time_of_day = 0.5f * (sin(elapsed_time * 0.1f) + 1.0f);

        // START HANDLE TERMINAL
        CHAR *typed_chars = (CHAR *)window.typed_chars.data;
        WriteFile(window.console.input, typed_chars, window.typed_chars.length, NULL, NULL);

        // Exit when the process we're reading from exits.
        if (WaitForSingleObject(window.console.h_process, 0) != WAIT_TIMEOUT) {
            break;
        }

        DWORD bytes_available;
        PeekNamedPipe(window.console.output, NULL, 0, NULL, &bytes_available, NULL);

        if (bytes_available > 0) {
            DWORD dwRead;
            BOOL did_read = ReadFile(window.console.output, read_buffer, READ_BUFFER_SIZE, &dwRead, NULL);
            if (did_read && dwRead != 0) {
                size_t out;
                mbstowcs_s(&out, data.text, data_text_capacity, read_buffer, dwRead);
                data.textLength = dwRead;

                for (size_t i = 0; i < data.textLength;) {
                    // Check for escape sequences.
                    // https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences If <n>
                    // is omitted for colors, it is assumed to be 0, if <x,y,n> are omitted for positioning, they
                    // are assumed to be 1.
                    if (grid_parse_escape_sequence(&grid, &data, &i, &window)) {
                        continue;
                    }

                    // Parse escape characters:
                    if (data_match_char(&data, L'\r', &i)) {
                        grid_cursor_move_to(&grid, 0, grid.cursor_y);
                        continue;
                    }

                    if (data_match_char(&data, L'\n', &i)) {
                        if (grid.cursor_y == grid.height - 1) {
                            grid_scroll_down(&grid);
                        } else {
                            grid.cursor_y++;
                        }
                        continue;
                    }

                    if (data_match_char(&data, L'\b', &i)) {
                        grid_cursor_move(&grid, -1, 0);
                        continue;
                    }

                    if (grid.cursor_x >= grid.width) {
                        grid.cursor_x = 0;
                        grid.cursor_y++;
                    }
                    grid_set_char(&grid, grid.cursor_x, grid.cursor_y, data.text[i]);
                    grid.cursor_x++;

                    i++;
                }
            }
        }

        sprite_batch_begin(&sprite_batch);

        for (size_t y = 0; y < grid.height; y++) {
            for (size_t x = 0; x < grid.width; x++) {
                grid_draw_tile(&grid, &sprite_batch, x, y, 0, window.height);
            }
        }

        if (grid.show_cursor) {
            grid_draw_cursor(&grid, &sprite_batch, grid.cursor_x, grid.cursor_y, 2, window.height);
        }

        sprite_batch_end(&sprite_batch, texture_atlas_2d.width, texture_atlas_2d.height);

        // END HANDLE TERMINAL

        // Draw:
        glClearColor(sky_color_r * time_of_day, sky_color_g * time_of_day, sky_color_b * time_of_day, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program_2d);
        glUniformMatrix4fv(projection_matrix_location_2d, 1, GL_FALSE, (const float *)&projection_matrix_2d);
        glBindTexture(GL_TEXTURE_2D, texture_atlas_2d.id);
        sprite_batch_draw(&sprite_batch);

        window_update(&window);

        glfwSwapBuffers(window.glfw_window);
        glfwPollEvents();
    }

    // TODO: Console should use a _create, _destroy convention.
    ClosePseudoConsole(window.console.hpc);

    grid_destroy(&grid);
    data_destroy(&data);

    sprite_batch_destroy(&sprite_batch);

    glDeleteTextures(1, &texture_atlas_2d.id);

    window_destroy(&window);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
