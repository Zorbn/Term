#include "renderer.h"

#include "../font.h"

struct Renderer renderer_create(struct Grid *grid) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    struct Renderer renderer = (struct Renderer){
        .scale = 1,
        .background_color = color_from_hex(GRID_COLOR_BACKGROUND_DEFAULT),

        .program = program_create("assets/shader_2d.vert", "assets/shader_2d.frag"),
        // TODO: Add texture_bind and texture_destroy
        .texture_atlas = texture_create("assets/texture_atlas.png"),
    };

    renderer.projection_matrix_location = glGetUniformLocation(renderer.program, "projection_matrix");
    renderer.offset_y_location = glGetUniformLocation(renderer.program, "offset_y");

    renderer_resize(&renderer, grid, renderer.scale);

    return renderer;
}

void renderer_draw(struct Renderer *renderer, struct Grid *grid, int32_t origin_y, GLFWwindow *glfw_window) {
    glClearColor(renderer->background_color.r, renderer->background_color.g, renderer->background_color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(renderer->program);
    glUniformMatrix4fv(renderer->projection_matrix_location, 1, GL_FALSE, (const float *)&renderer->projection_matrix);
    glBindTexture(GL_TEXTURE_2D, renderer->texture_atlas.id);

    for (size_t y = 0; y < renderer->sprite_batch_count; y++) {
        struct SpriteBatch *sprite_batch = &renderer->sprite_batches[y];

        if (grid->are_rows_dirty[y]) {
            grid->are_rows_dirty[y] = false;

            sprite_batch_begin(sprite_batch);

            for (size_t x = 0; x < grid->width; x++) {
                grid_draw_tile(grid, sprite_batch, x, y, 0, renderer->scale);
            }

            if (grid->should_show_cursor && y == grid->cursor_y) {
                grid_draw_cursor(grid, sprite_batch, grid->cursor_x, grid->cursor_y, 2, renderer->scale);
            }

            sprite_batch_end(sprite_batch, renderer->texture_atlas.width, renderer->texture_atlas.height);
        }

        float offset_y = origin_y - (y + 1) * FONT_GLYPH_HEIGHT * renderer->scale;
        glUniform1f(renderer->offset_y_location, offset_y);
        sprite_batch_draw(sprite_batch);
    }

    glfwSwapBuffers(glfw_window);
}

void renderer_resize_viewport(struct Renderer *renderer, int32_t width, int32_t height) {
    glViewport(0, 0, width, height);
    renderer->projection_matrix = glms_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0, 100.0);
}

void renderer_resize(struct Renderer *renderer, struct Grid *grid, float scale) {
    renderer->scale = scale;

    if (renderer->sprite_batches) {
        for (size_t i = 0; i < renderer->sprite_batch_count; i++) {
            sprite_batch_destroy(&renderer->sprite_batches[i]);
        }
        free(renderer->sprite_batches);
    }

    renderer->sprite_batch_count = grid->height;
    renderer->sprite_batches = malloc(renderer->sprite_batch_count * sizeof(struct SpriteBatch));

    for (size_t i = 0; i < renderer->sprite_batch_count; i++) {
        // 2 sprites per tile (foreground background), plus a potential cursor which also has a foreground and background.
        renderer->sprite_batches[i] = sprite_batch_create(grid->width * 2 + 2);
    }
}

void renderer_scroll_down(struct Renderer* renderer) {
    // Rotate the sprite batchs to allow scrolling without updating every batch.
    struct SpriteBatch first_sprite_batch = renderer->sprite_batches[0];
    memmove(&renderer->sprite_batches[0], &renderer->sprite_batches[1], (renderer->sprite_batch_count - 1) * sizeof(struct SpriteBatch));
    renderer->sprite_batches[renderer->sprite_batch_count - 1] = first_sprite_batch;
}

void renderer_destroy(struct Renderer *renderer) {
    for (size_t i = 0; i < renderer->sprite_batch_count; i++) {
        sprite_batch_destroy(&renderer->sprite_batches[i]);
    }
    free(renderer->sprite_batches);

    glDeleteTextures(1, &renderer->texture_atlas.id);
}