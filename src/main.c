#include "detect_leak.h"

#include "window.h"
#include "grid.h"
#include "text_buffer.h"
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
 */

/*
 * Missing features:
 * Mouse input,
 * Copy/paste,
 * Scrollback,
 */

const float sky_color_r = 50.0f / 255.0f;
const float sky_color_g = 74.0f / 255.0f;
const float sky_color_b = 117.0f / 255.0f;

int main() {
    struct Window window = window_create("CBlock", 640, 480);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    uint32_t program_2d = program_create("assets/shader_2d.vert", "assets/shader_2d.frag");

    // TODO: Add texture_bind and texture_destroy
    struct Texture texture_atlas_2d = texture_create("assets/texture_atlas.png");

    mat4s projection_matrix_2d;

    int32_t projection_matrix_location_2d = glGetUniformLocation(program_2d, "projection_matrix");

    struct TextBuffer data = text_buffer_create();

    const int32_t grid_width = window.width / 6;
    const int32_t grid_height = window.height / 14;
    window.pseudo_console = pseudo_console_create((COORD){grid_width, grid_height});
    printf("Console result: %ld\n", window.pseudo_console.result);
    struct Grid grid = grid_create(grid_width, grid_height);
    struct SpriteBatch sprite_batch = sprite_batch_create(grid.size * 2);

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
            // printf("fps: %f\n", 1.0f / delta_time);
        }

        elapsed_time += delta_time;
        float time_of_day = 0.5f * (sin(elapsed_time * 0.1f) + 1.0f);

        CHAR *typed_chars = (CHAR *)window.typed_chars.data;
        WriteFile(window.pseudo_console.input, typed_chars, window.typed_chars.length, NULL, NULL);

        // Exit when the process we're reading from exits.
        if (WaitForSingleObject(window.pseudo_console.h_process, 0) != WAIT_TIMEOUT) {
            break;
        }

        DWORD bytes_available;
        PeekNamedPipe(window.pseudo_console.output, NULL, 0, NULL, &bytes_available, NULL);

        if (bytes_available > 0) {
            BOOL did_read = ReadFile(window.pseudo_console.output, data.data, TEXT_BUFFER_CAPACITY, &data.length, NULL);
            if (did_read) {
                for (size_t i = 0; i < data.length;) {
                    // Skip multi-byte text. Replace it with a box character.
                    if (data.data[i] & 0x80) {
                        size_t j = 0;
                        while (j < 4 && ((data.data[i] << j) & 0x80)) {
                            j++;
                        }
                        i += j - 1;
                        data.data[i] = 127;
                    }

                    // Check for escape sequences.
                    // https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences If <n>
                    // is omitted for colors, it is assumed to be 0, if <x,y,n> are omitted for positioning, they
                    // are assumed to be 1.
                    if (grid_parse_escape_sequence(&grid, &data, &i, &window)) {
                        continue;
                    }

                    // Parse escape characters:
                    if (text_buffer_match_char(&data, '\r', &i)) {
                        grid_cursor_move_to(&grid, 0, grid.cursor_y);
                        continue;
                    }

                    if (text_buffer_match_char(&data, '\n', &i)) {
                        if (grid.cursor_y == grid.height - 1) {
                            grid_scroll_down(&grid);
                        } else {
                            grid.cursor_y++;
                        }
                        continue;
                    }

                    if (text_buffer_match_char(&data, '\b', &i)) {
                        grid_cursor_move(&grid, -1, 0);
                        continue;
                    }

                    if (grid.cursor_x >= grid.width) {
                        grid.cursor_x = 0;
                        grid.cursor_y++;
                    }

                    grid_set_char(&grid, grid.cursor_x, grid.cursor_y, data.data[i]);
                    grid.cursor_x++;

                    i++;
                }
            }
        }

        // Draw:
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

    pseudo_console_destroy(&window.pseudo_console);

    grid_destroy(&grid);
    text_buffer_destroy(&data);

    sprite_batch_destroy(&sprite_batch);

    glDeleteTextures(1, &texture_atlas_2d.id);

    window_destroy(&window);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
