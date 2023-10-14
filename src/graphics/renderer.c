#include "renderer.h"

#include "../font.h"
#include <stdlib.h>

struct Renderer renderer_create(size_t width, size_t height) {
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

    renderer_resize(&renderer, width, height, renderer.scale);

    return renderer;
}

void renderer_on_row_changed(void *context, int32_t y) {
    struct Renderer *renderer = context;
    int32_t sprite_batch_y = y + renderer->scrollback_distance;

    if (sprite_batch_y >= renderer->sprite_batch_count) {
        return;
    }

    renderer->are_sprite_batches_dirty[sprite_batch_y] = true;
}

int32_t renderer_get_visible_scrollback_line_count(struct Renderer *renderer) {
    int32_t visible_scrollback_line_count = renderer->scrollback_distance;
    if (visible_scrollback_line_count > renderer->sprite_batch_count) {
        visible_scrollback_line_count = renderer->sprite_batch_count;
    }

    return visible_scrollback_line_count;
}

void renderer_draw_character(char character, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z,
    float scale, float r, float g, float b) {

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

void renderer_draw_box(struct SpriteBatch *sprite_batch, int32_t x, int32_t z, float scale, float r, float g, float b) {
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

void renderer_draw_tile(char character, struct Color foreground_color, struct Color background_color,
    struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, float scale) {

    renderer_draw_box(sprite_batch, x, z, scale, background_color.r, background_color.g, background_color.b);
    renderer_draw_character(
        character, sprite_batch, x, y, z + 1, scale, foreground_color.r, foreground_color.g, foreground_color.b);
}

void renderer_draw_cursor(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, float scale) {

    renderer_draw_box(sprite_batch, x, z, scale, 1.0f, 1.0f, 1.0f);

    char character = grid->data[x + y * grid->width];
    renderer_draw_character(character, sprite_batch, x, y, z + 1, scale, 0.0f, 0.0f, 0.0f);
}

void renderer_draw_scrollback(struct Renderer *renderer, struct Grid *grid, int32_t visible_scrollback_line_count) {
    for (size_t y = 0; y < visible_scrollback_line_count; y++) {
        if (!renderer->are_sprite_batches_dirty[y]) {
            continue;
        }
        renderer->are_sprite_batches_dirty[y] = false;

        struct SpriteBatch *sprite_batch = &renderer->sprite_batches[y];

        sprite_batch_begin(sprite_batch);

        size_t scrollback_y = grid->scrollback_lines.length - renderer->scrollback_distance + y;
        size_t scrollback_line_length = grid->scrollback_lines.data[scrollback_y].length;
        if (scrollback_line_length > grid->width) {
            scrollback_line_length = grid->width;
        }

        for (size_t x = 0; x < scrollback_line_length; x++) {
            char character = grid->scrollback_lines.data[scrollback_y].data[x];
            struct Color background_color =
                color_from_hex(grid->scrollback_lines.data[scrollback_y].background_colors[x]);
            struct Color foreground_color =
                color_from_hex(grid->scrollback_lines.data[scrollback_y].foreground_colors[x]);

            renderer_draw_tile(character, foreground_color, background_color, sprite_batch, x, y, 0, renderer->scale);
        }

        sprite_batch_end(sprite_batch, renderer->texture_atlas.width, renderer->texture_atlas.height);
    }
}

void renderer_draw_grid(struct Renderer *renderer, struct Grid *grid, int32_t visible_scrollback_line_count) {
    for (size_t y = visible_scrollback_line_count; y < renderer->sprite_batch_count; y++) {
        if (!renderer->are_sprite_batches_dirty[y]) {
            continue;
        }
        renderer->are_sprite_batches_dirty[y] = false;

        struct SpriteBatch *sprite_batch = &renderer->sprite_batches[y];
        size_t grid_y = y - renderer->scrollback_distance;

        sprite_batch_begin(sprite_batch);

        for (size_t x = 0; x < grid->width; x++) {
            size_t i = x + grid_y * grid->width;
            char character = grid->data[x + grid_y * grid->width];
            struct Color background_color = color_from_hex(grid->background_colors[i]);
            struct Color foreground_color = color_from_hex(grid->foreground_colors[i]);

            renderer_draw_tile(
                character, foreground_color, background_color, sprite_batch, x, y, 0, renderer->scale);
        }

        if (grid->should_show_cursor && grid_y == grid->cursor_y) {
            renderer_draw_cursor(grid, sprite_batch, grid->cursor_x, grid->cursor_y, 2, renderer->scale);
        }

        sprite_batch_end(sprite_batch, renderer->texture_atlas.width, renderer->texture_atlas.height);
    }
}

void renderer_draw(struct Renderer *renderer, struct Grid *grid, int32_t origin_y, GLFWwindow *glfw_window) {
    glClearColor(renderer->background_color.r, renderer->background_color.g, renderer->background_color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(renderer->program);
    glUniformMatrix4fv(renderer->projection_matrix_location, 1, GL_FALSE, (const float *)&renderer->projection_matrix);
    glBindTexture(GL_TEXTURE_2D, renderer->texture_atlas.id);

    int32_t visible_scrollback_line_count = renderer_get_visible_scrollback_line_count(renderer);
    renderer_draw_scrollback(renderer, grid, visible_scrollback_line_count);
    renderer_draw_grid(renderer, grid, visible_scrollback_line_count);

    for (size_t y = 0; y < renderer->sprite_batch_count; y++) {
        struct SpriteBatch *sprite_batch = &renderer->sprite_batches[y];

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

void renderer_mark_all_sprite_batches_dirty(struct Renderer *renderer) {
    for (size_t i = 0; i < renderer->sprite_batch_count; i++) {
        renderer->are_sprite_batches_dirty[i] = true;
    }
}

void renderer_resize(struct Renderer *renderer, size_t width, size_t height, float scale) {
    renderer->scale = scale;

    for (size_t i = 0; i < renderer->sprite_batch_count; i++) {
        sprite_batch_destroy(&renderer->sprite_batches[i]);
    }

    renderer->sprite_batch_count = height;
    free(renderer->sprite_batches);
    renderer->sprite_batches = malloc(renderer->sprite_batch_count * sizeof(struct SpriteBatch));
    assert(renderer->sprite_batches);
    free(renderer->are_sprite_batches_dirty);
    renderer->are_sprite_batches_dirty = malloc(renderer->sprite_batch_count * sizeof(bool));
    assert(renderer->are_sprite_batches_dirty);
    // Every row is dirty when the screen gets resized.
    renderer_mark_all_sprite_batches_dirty(renderer);

    for (size_t i = 0; i < renderer->sprite_batch_count; i++) {
        // 2 sprites per tile (foreground background), plus a potential cursor which also has a foreground and
        // background.
        renderer->sprite_batches[i] = sprite_batch_create(width * 2 + 2);
    }
}

void renderer_scroll_reset(struct Renderer *renderer) {
    if (renderer->scrollback_distance == 0) {
        return;
    }

    renderer->scrollback_distance = 0;
    renderer_mark_all_sprite_batches_dirty(renderer);
}

void renderer_scroll_down(struct Renderer *renderer, bool is_scrolling_with_grid) {
    if (!is_scrolling_with_grid) {
        renderer->scrollback_distance -= 1;
        if (renderer->scrollback_distance < 0) {
            renderer->scrollback_distance = 0;
            return;
        }
    }

    // Rotate the sprite batchs to allow scrolling without updating every batch.
    struct SpriteBatch first_sprite_batch = renderer->sprite_batches[0];
    memmove(&renderer->sprite_batches[0], &renderer->sprite_batches[1],
        (renderer->sprite_batch_count - 1) * sizeof(struct SpriteBatch));
    renderer->sprite_batches[renderer->sprite_batch_count - 1] = first_sprite_batch;

    memmove(renderer->are_sprite_batches_dirty, renderer->are_sprite_batches_dirty + 1,
        (renderer->sprite_batch_count - 1) * sizeof(bool));
    // Now the bottom sprite batch is the only one with newly outdated content.
    renderer->are_sprite_batches_dirty[renderer->sprite_batch_count - 1] = true;
}

void renderer_scroll_up(struct Renderer *renderer, struct Grid *grid) {
    renderer->scrollback_distance += 1;
    if (renderer->scrollback_distance > grid->scrollback_lines.length) {
        renderer->scrollback_distance = grid->scrollback_lines.length;
        return;
    }

    // Rotate the sprite batchs to allow scrolling without updating every batch.
    struct SpriteBatch last_sprite_batch = renderer->sprite_batches[renderer->sprite_batch_count - 1];
    memmove(&renderer->sprite_batches[1], &renderer->sprite_batches[0],
        (renderer->sprite_batch_count - 1) * sizeof(struct SpriteBatch));
    renderer->sprite_batches[0] = last_sprite_batch;

    memmove(renderer->are_sprite_batches_dirty + 1, renderer->are_sprite_batches_dirty,
        (renderer->sprite_batch_count - 1) * sizeof(bool));
    // Now the top sprite batch is the only one with newly outdated content.
    renderer->are_sprite_batches_dirty[0] = true;
}

void renderer_destroy(struct Renderer *renderer) {
    for (size_t i = 0; i < renderer->sprite_batch_count; i++) {
        sprite_batch_destroy(&renderer->sprite_batches[i]);
    }
    free(renderer->sprite_batches);
    free(renderer->are_sprite_batches_dirty);

    glDeleteTextures(1, &renderer->texture_atlas.id);
}