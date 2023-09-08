#include "renderer.h"

const float sky_color_r = 50.0f / 255.0f;
const float sky_color_g = 74.0f / 255.0f;
const float sky_color_b = 117.0f / 255.0f;

struct Renderer renderer_create(struct Grid *grid) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    struct Renderer renderer = (struct Renderer){
        .program = program_create("assets/shader_2d.vert", "assets/shader_2d.frag"),
        // TODO: Add texture_bind and texture_destroy
        .texture_atlas = texture_create("assets/texture_atlas.png"),
        .sprite_batch = sprite_batch_create(grid->size * 2),
    };

    renderer.projection_matrix_location = glGetUniformLocation(renderer.program, "projection_matrix");

    return renderer;
}

void renderer_draw(struct Renderer *renderer, struct Grid *grid, int32_t origin_y, GLFWwindow* glfw_window) {
    sprite_batch_begin(&renderer->sprite_batch);

    for (size_t y = 0; y < grid->height; y++) {
        for (size_t x = 0; x < grid->width; x++) {
            grid_draw_tile(grid, &renderer->sprite_batch, x, y, 0, origin_y);
        }
    }

    if (grid->show_cursor) {
        grid_draw_cursor(grid, &renderer->sprite_batch, grid->cursor_x, grid->cursor_y, 2, origin_y);
    }

    sprite_batch_end(&renderer->sprite_batch, renderer->texture_atlas.width, renderer->texture_atlas.height);

    glClearColor(sky_color_r, sky_color_g, sky_color_b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(renderer->program);
    glUniformMatrix4fv(renderer->projection_matrix_location, 1, GL_FALSE, (const float *)&renderer->projection_matrix);
    glBindTexture(GL_TEXTURE_2D, renderer->texture_atlas.id);
    sprite_batch_draw(&renderer->sprite_batch);

    glfwSwapBuffers(glfw_window);
}

void renderer_resize_viewport(struct Renderer *renderer, int32_t width, int32_t height) {
    glViewport(0, 0, width, height);
    renderer->projection_matrix = glms_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0, 100.0);
}

void renderer_resize(struct Renderer *renderer, struct Grid *grid) {
    sprite_batch_destroy(&renderer->sprite_batch);
    renderer->sprite_batch = sprite_batch_create(grid->size * 2);
}

void renderer_destroy(struct Renderer *renderer) {
    sprite_batch_destroy(&renderer->sprite_batch);
    glDeleteTextures(1, &renderer->texture_atlas.id);
}