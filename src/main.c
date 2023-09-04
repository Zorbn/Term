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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define BLOCK_TEXTURE_COUNT 3

const float sky_color_r = 100.0f / 255.0f;
const float sky_color_g = 149.0f / 255.0f;
const float sky_color_b = 237.0f / 255.0f;

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

        sprite_batch_begin(&sprite_batch);
        sprite_batch_add(&sprite_batch, (struct Sprite){
                                            .x = 0,
                                            .y = window.height - 14,
                                            .width = 6,
                                            .height = 14,
                                            .texture_x = 16,
                                            .texture_width = 6,
                                            .texture_height = 14,
                                        });
        sprite_batch_end(&sprite_batch, texture_atlas_2d.width, texture_atlas_2d.height);

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

    sprite_batch_destroy(&sprite_batch);

    glDeleteTextures(1, &texture_atlas_2d.id);

    window_destroy(&window);

    printf("Found leaks: %s\n", _CrtDumpMemoryLeaks() ? "true" : "false");

    return 0;
}
