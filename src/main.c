// #pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#include "detect_leak.h"

#include "window.h"
#include "grid.h"
#include "text_buffer.h"
#include "font.h"
#include "graphics/renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

/*
 * Missing features:
 * Mouse input,
 * Copy/paste,
 * Scrollback,
 */

int main() {
    struct Window window = window_create("Term", 640, 480);

    const int32_t grid_width = window.width / FONT_GLYPH_WIDTH;
    const int32_t grid_height = window.height / FONT_GLYPH_HEIGHT;
    window.pseudo_console = pseudo_console_create((COORD){grid_width, grid_height});
    printf("Console result: %ld\n", window.pseudo_console.result);

    struct Grid grid = grid_create(grid_width, grid_height);
    struct TextBuffer text_buffer = text_buffer_create();
    struct Renderer renderer = renderer_create(&grid);
    window_setup(&window, &grid, &renderer);

    double last_frame_time = glfwGetTime();
    float fps_print_timer = 0.0f;

    while (!glfwWindowShouldClose(window.glfw_window)) {
        if (window.did_resize) {
            size_t new_grid_width = window.width / (window.scale * FONT_GLYPH_WIDTH);
            size_t new_grid_height = window.height / (window.scale * FONT_GLYPH_HEIGHT);

            if (new_grid_width < 1) {
                new_grid_width = 1;
            }

            if (new_grid_height < 1) {
                new_grid_height = 1;
            }

            pseudo_console_resize(&window.pseudo_console, new_grid_width, new_grid_height);
            grid_resize(window.grid, new_grid_width, new_grid_height);
            renderer_resize(&renderer, &grid, window.scale);

            window.did_resize = false;
        }

        double current_frame_time = glfwGetTime();
        float delta_time = (float)(current_frame_time - last_frame_time);
        last_frame_time = current_frame_time;

        fps_print_timer += delta_time;

        if (fps_print_timer > 1.0f) {
            fps_print_timer = 0.0f;
            printf("fps: %f\n", 1.0f / delta_time);
        }

        if (window.typed_chars.length > 0) {
            CHAR *typed_chars = (CHAR *)window.typed_chars.data;
            WriteFile(window.pseudo_console.input, typed_chars, window.typed_chars.length, NULL, NULL);
        }

        // Exit when the process we're reading from exits.
        if (WaitForSingleObject(window.pseudo_console.h_process, 0) != WAIT_TIMEOUT) {
            break;
        }

        DWORD bytes_available;
        while (
            PeekNamedPipe(window.pseudo_console.output, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
            BOOL did_read = ReadFile(
                window.pseudo_console.output, text_buffer.data + text_buffer.kept_length, TEXT_BUFFER_CAPACITY - text_buffer.kept_length, &text_buffer.length, NULL);
            text_buffer.length += text_buffer.kept_length;
            text_buffer.kept_length = 0;
            if (did_read) {
                for (size_t i = 0; i < text_buffer.length;) {
                    // Skip multi-byte text. Replace it with a box character.
                    if (text_buffer.data[i] & 0x80) {
                        size_t j = 0;
                        while (j < 4 && ((text_buffer.data[i] << j) & 0x80)) {
                            j++;
                        }

                        size_t end_i = i + j - 1;
                        if (end_i >= text_buffer.length) {
                            // The multi-byte utf8 character was split across multiple reads.
                            text_buffer_keep_from_i(&text_buffer, i);
                            break;
                        }

                        i = end_i;
                        text_buffer.data[i] = FONT_LENGTH + 32;
                    }

                    // Check for escape sequences.
                    // https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences If <n>
                    // is omitted for colors, it is assumed to be 0, if <x,y,n> are omitted for positioning, they
                    // are assumed to be 1.
                    size_t furthest_i = 0;
                    if (grid_parse_escape_sequence(&grid, &text_buffer, &i, &furthest_i, &window)) {
                        continue;
                    } else if (furthest_i >= text_buffer.length && i < text_buffer.length) {
                        // The parse failed due to reaching the end of the buffer, the sequence may have been split across multiple reads.
                        text_buffer_keep_from_i(&text_buffer, i);
                        break;
                    }

                    // Parse escape characters:
                    if (text_buffer_match_char(&text_buffer, '\r', &i)) {
                        grid_cursor_move_to(&grid, 0, grid.cursor_y);
                        continue;
                    }

                    if (text_buffer_match_char(&text_buffer, '\n', &i)) {
                        if (grid.cursor_y == grid.height - 1) {
                            renderer_scroll_down(&renderer);
                            grid_scroll_down(&grid);
                        } else {
                            grid_cursor_move(&grid, 0, 1);
                        }
                        continue;
                    }

                    if (text_buffer_match_char(&text_buffer, '\b', &i)) {
                        grid_cursor_move(&grid, -1, 0);
                        continue;
                    }

                    // \a is the alert/bell escape sequence, ignore it.
                    if (text_buffer_match_char(&text_buffer, '\a', &i)) {
                        continue;
                    }

                    if (grid.cursor_x >= grid.width) {
                        grid_cursor_move_to(&grid, 0, grid.cursor_y + 1);
                    }
                    grid_set_char(&grid, grid.cursor_x, grid.cursor_y, text_buffer.data[i]);
                    grid.cursor_x++;

                    i++;
                }
            }
        }

        renderer_draw(&renderer, &grid, window.height, window.glfw_window);
        window_update(&window);
        glfwPollEvents();
    }

    pseudo_console_destroy(&window.pseudo_console, &text_buffer);
    window_destroy(&window);
    grid_destroy(&grid);
    text_buffer_destroy(&text_buffer);
    renderer_destroy(&renderer);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
