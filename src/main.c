#include "detect_leak.h"

#include "window.h"
#include "grid.h"
#include "text_buffer.h"

#include "graphics/renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

/*
 * TODO:
 * Dynamic resizing.
 */

/*
 * Missing features:
 * Mouse input,
 * Copy/paste,
 * Scrollback,
 */

int main() {
    struct Window window = window_create("CBlock", 640, 480);

    const int32_t grid_width = window.width / 6;
    const int32_t grid_height = window.height / 14;
    window.pseudo_console = pseudo_console_create((COORD){grid_width, grid_height});
    printf("Console result: %ld\n", window.pseudo_console.result);

    struct Grid grid = grid_create(grid_width, grid_height);
    struct TextBuffer text_buffer = text_buffer_create();
    struct Renderer renderer = renderer_create(&grid);
    window_setup_resize_callback(&window, &grid, &renderer);

    double last_frame_time = glfwGetTime();
    float fps_print_timer = 0.0f;

    while (!glfwWindowShouldClose(window.glfw_window)) {
        if (window.did_resize) {
            // TODO: Don't hard code font glyph size anywhere.
            const size_t new_grid_width = window.width / 6;
            const size_t new_grid_height = window.height / 14;

            pseudo_console_resize(&window.pseudo_console, new_grid_width, new_grid_height);
            grid_resize(window.grid, new_grid_width, new_grid_height);
            renderer_resize(&renderer, &grid);

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

        CHAR *typed_chars = (CHAR *)window.typed_chars.data;
        WriteFile(window.pseudo_console.input, typed_chars, window.typed_chars.length, NULL, NULL);

        // Exit when the process we're reading from exits.
        if (WaitForSingleObject(window.pseudo_console.h_process, 0) != WAIT_TIMEOUT) {
            break;
        }

        DWORD bytes_available;
        PeekNamedPipe(window.pseudo_console.output, NULL, 0, NULL, &bytes_available, NULL);

        if (bytes_available > 0) {
            BOOL did_read = ReadFile(window.pseudo_console.output, text_buffer.data, TEXT_BUFFER_CAPACITY, &text_buffer.length, NULL);
            if (did_read) {
                for (size_t i = 0; i < text_buffer.length;) {
                    // Skip multi-byte text. Replace it with a box character.
                    if (text_buffer.data[i] & 0x80) {
                        size_t j = 0;
                        while (j < 4 && ((text_buffer.data[i] << j) & 0x80)) {
                            j++;
                        }
                        i += j - 1;
                        text_buffer.data[i] = 127;
                    }

                    // Check for escape sequences.
                    // https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences If <n>
                    // is omitted for colors, it is assumed to be 0, if <x,y,n> are omitted for positioning, they
                    // are assumed to be 1.
                    if (grid_parse_escape_sequence(&grid, &text_buffer, &i, &window)) {
                        continue;
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

                    if (grid.cursor_x >= grid.width) {
                        grid.cursor_x = 0;
                        grid.cursor_y++;
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

    pseudo_console_destroy(&window.pseudo_console);
    window_destroy(&window);
    grid_destroy(&grid);
    text_buffer_destroy(&text_buffer);
    renderer_destroy(&renderer);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
