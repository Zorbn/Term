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
    window.pseudo_console = pseudo_console_create((COORD){grid_width, grid_height});

    struct Renderer renderer = renderer_create(grid_width, grid_height);
    struct Grid grid = grid_create(grid_width, grid_height, &renderer, renderer_on_row_changed);
    window_setup(&window, &grid, &renderer);

    struct ReadThreadData read_thread_data = (struct ReadThreadData){
        .grid = &grid,
        .renderer = &renderer,
        .window = &window,
        .mutex = CreateMutex(NULL, false, NULL),
        .event = CreateEvent(NULL, false, false, NULL),
    };
    assert(read_thread_data.mutex);

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

            WaitForSingleObject(read_thread_data.mutex, INFINITE);

            pseudo_console_resize(&window.pseudo_console, new_grid_width, new_grid_height);
            renderer_resize(&renderer, new_grid_width, new_grid_height, window.scale);
            grid_resize(window.grid, new_grid_width, new_grid_height);

            window.did_resize = false;

            ReleaseMutex(read_thread_data.mutex);
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

        if (window.needs_redraw) {
            WaitForSingleObject(read_thread_data.mutex, INFINITE);

            window_show(&window);
            renderer_draw(&renderer, &grid, window.height, window.glfw_window);

            window.needs_redraw = false;

            ReleaseMutex(read_thread_data.mutex);
        }

        window_update(&window);

        MsgWaitForMultipleObjects(1, &read_thread_data.event, false, INFINITE, QS_ALLINPUT);

        glfwPollEvents();
    }

    pseudo_console_destroy(&window.pseudo_console);
    reader_destroy(&reader, &read_thread_data);
    window_destroy(&window);
    grid_destroy(&grid);
    renderer_destroy(&renderer);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
