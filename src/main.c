// #pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#include "detect_leak.h"

#include "window.h"
#include "grid.h"
#include "text_buffer.h"
#include "font.h"
#include "reader.h"
#include "graphics/renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

/*
 * Missing features:
 * Copy/paste,
 */

int main(void) {
    struct Window window = window_create("Term", 640, 480);

    const int32_t grid_width = window.width / FONT_GLYPH_WIDTH;
    const int32_t grid_height = window.height / FONT_GLYPH_HEIGHT;

    struct PseudoConsole pseudo_console = pseudo_console_create((COORD){grid_width, grid_height});
    struct Renderer renderer = renderer_create(grid_width, grid_height);
    struct Grid grid = grid_create(grid_width, grid_height, &renderer, renderer_on_row_changed_callback);

    window_setup(&window, &grid, &renderer);

    struct ReadThreadData read_thread_data = read_thread_data_create(&pseudo_console, &grid, &renderer);
    struct Reader reader = reader_create(&read_thread_data);

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

            read_thread_data_lock(&read_thread_data);

            pseudo_console_resize(&pseudo_console, new_grid_width, new_grid_height);
            renderer_resize(&renderer, new_grid_width, new_grid_height, window.scale);
            grid_resize(window.grid, new_grid_width, new_grid_height);

            read_thread_data_unlock(&read_thread_data);

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
            WriteFile(pseudo_console.input, typed_chars, window.typed_chars.length, NULL, NULL);
        }

        // Exit when the process we're reading from exits.
        if (WaitForSingleObject(pseudo_console.h_process, 0) != WAIT_TIMEOUT) {
            break;
        }

        if (renderer.needs_redraw) {
            read_thread_data_lock(&read_thread_data);

            window_show(&window);
            renderer_draw(&renderer, &grid, window.height, &window);

            renderer.needs_redraw = false;

            read_thread_data_unlock(&read_thread_data);
        }

        if (read_thread_data.title_buffer.is_dirty) {
            read_thread_data_lock(&read_thread_data);

            window_set_title(&window, read_thread_data.title_buffer.data);

            read_thread_data.title_buffer.is_dirty = false;

            read_thread_data_unlock(&read_thread_data);
        }

        window_update(&window);

        // Pause until we get an update from the pseudo console, the reader, or window input.
        HANDLE handles[2] = {pseudo_console.h_process, read_thread_data.event};
        MsgWaitForMultipleObjects(2, handles, false, INFINITE, QS_ALLINPUT);

        read_thread_data_lock(&read_thread_data);
        glfwPollEvents();
        read_thread_data_unlock(&read_thread_data);
    }

    pseudo_console_destroy(&pseudo_console);
    reader_destroy(&reader);
    read_thread_data_destroy(&read_thread_data);
    window_destroy(&window);
    grid_destroy(&grid);
    renderer_destroy(&renderer);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
