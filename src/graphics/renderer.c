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
        .texture_atlas = texture_create("assets/texture_atlas.png"),

        .needs_redraw = true,
    };

    renderer.projection_matrix_location = glGetUniformLocation(renderer.program, "projection_matrix");
    renderer.offset_y_location = glGetUniformLocation(renderer.program, "offset_y");

    renderer_resize(&renderer, width, height, renderer.scale);

    return renderer;
}

void renderer_on_row_changed_callback(void *context, int32_t y) {
    struct Renderer *renderer = context;
    renderer_on_row_changed(renderer, y);
}

void renderer_on_row_changed(struct Renderer *renderer, int32_t y) {
    int32_t sprite_batch_y = y + renderer->scrollback_distance;

    if (sprite_batch_y >= renderer->sprite_batch_count) {
        return;
    }

    renderer->are_sprite_batches_dirty[sprite_batch_y] = true;
    renderer->needs_redraw = true;
}

static void renderer_on_selection_changed(struct Renderer *renderer, struct Selection *old_selection) {
    struct Selection sorted_old_selection = selection_sorted(old_selection);
    struct Selection sorted_new_selection = selection_sorted(&renderer->selection);

    int32_t min_y = int32_min(sorted_old_selection.start_y, sorted_new_selection.start_y);
    int32_t max_y = int32_max(sorted_old_selection.end_y, sorted_new_selection.end_y);

    for (int32_t y = min_y; y <= max_y; y++) {
        renderer_on_row_changed(renderer, y);
    }
}

static void renderer_on_scroll(struct Renderer *renderer) {
    renderer_on_selection_changed(renderer, &renderer->selection);
}

void renderer_clear_selection(struct Renderer *renderer) {
    renderer_on_selection_changed(renderer, &renderer->selection);
    renderer->selection_state = SELECTION_STATE_NONE;
}

void renderer_set_selection_start(struct Renderer *renderer, uint32_t x, uint32_t y) {
    struct Selection old_selection = renderer->selection;

    renderer->selection.start_x = x;
    renderer->selection.start_y = y - renderer->scrollback_distance;

    renderer->selection_state = SELECTION_STATE_STARTED;

    renderer_on_selection_changed(renderer, &old_selection);
}

void renderer_set_selection_end(struct Renderer *renderer, uint32_t x, uint32_t y) {
    if (renderer->selection_state == SELECTION_STATE_NONE) {
        return;
    }

    struct Selection old_selection = renderer->selection;

    renderer->selection.end_x = x;
    renderer->selection.end_y = y - renderer->scrollback_distance;

    renderer->selection_state = SELECTION_STATE_FINISHED;

    renderer_on_selection_changed(renderer, &old_selection);
}

static int32_t renderer_get_visible_scrollback_line_count(struct Renderer *renderer) {
    int32_t visible_scrollback_line_count = renderer->scrollback_distance;
    if (visible_scrollback_line_count > renderer->sprite_batch_count) {
        visible_scrollback_line_count = renderer->sprite_batch_count;
    }

    return visible_scrollback_line_count;
}

static void renderer_draw_character(
    char character,
    struct SpriteBatch *sprite_batch,
    int32_t x,
    int32_t z,
    float scale,
    float r,
    float g,
    float b
) {
    if (character == ' ') {
        return;
    }

    sprite_batch_add(
        sprite_batch,
        (struct Sprite){
            .x = x * FONT_GLYPH_WIDTH * scale,
            .z = z * scale,
            .width = FONT_GLYPH_WIDTH * scale,
            .height = FONT_GLYPH_HEIGHT * scale,

            .texture_x = (FONT_GLYPH_WIDTH + FONT_GLYPH_PADDING) * (character - 32),
            .texture_width = FONT_GLYPH_WIDTH,
            .texture_height = FONT_GLYPH_HEIGHT,

            .r = r,
            .g = g,
            .b = b,
        }
    );
}

static void renderer_draw_box(
    struct SpriteBatch *sprite_batch, int32_t x, int32_t z, float scale, float r, float g, float b
) {
    sprite_batch_add(
        sprite_batch,
        (struct Sprite){
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
        }
    );
}

static void renderer_draw_bar(
    struct SpriteBatch *sprite_batch, int32_t x, int32_t z, float scale, float r, float g, float b
) {
    sprite_batch_add(
        sprite_batch,
        (struct Sprite){
            .x = x * FONT_GLYPH_WIDTH * scale,
            .z = z * scale,
            .width = FONT_LINE_WIDTH * scale,
            .height = FONT_GLYPH_HEIGHT * scale,

            .texture_x = 0,
            .texture_width = FONT_LINE_WIDTH,
            .texture_height = FONT_GLYPH_HEIGHT,

            .r = r,
            .g = g,
            .b = b,
        }
    );
}

static void renderer_draw_underline(
    struct SpriteBatch *sprite_batch, int32_t x, int32_t z, float scale, float r, float g, float b
) {
    sprite_batch_add(
        sprite_batch,
        (struct Sprite){
            .x = x * FONT_GLYPH_WIDTH * scale,
            .z = (z + FONT_GLYPH_HEIGHT - FONT_LINE_WIDTH) * scale,
            .width = FONT_GLYPH_WIDTH * scale,
            .height = FONT_LINE_WIDTH * scale,

            .texture_x = 0,
            .texture_width = FONT_GLYPH_WIDTH,
            .texture_height = FONT_LINE_WIDTH,

            .r = r,
            .g = g,
            .b = b,
        }
    );
}

static void renderer_draw_tile(
    char character,
    struct Color foreground_color,
    struct Color background_color,
    struct SpriteBatch *sprite_batch,
    int32_t x,
    int32_t z,
    float scale
) {

    renderer_draw_box(sprite_batch, x, z, scale, background_color.r, background_color.g, background_color.b);
    renderer_draw_character(
        character,
        sprite_batch,
        x,
        z + 1,
        scale,
        foreground_color.r,
        foreground_color.g,
        foreground_color.b
    );
}

static void renderer_draw_cursor(
    struct Grid *grid, struct SpriteBatch *sprite_batch, int32_t x, int32_t y, int32_t z, float scale
) {

    if (x >= grid->width) {
        x = grid->width - 1;
    }

    switch (grid->cursor_style) {
        case GRID_CURSOR_STYLE_BLOCK: {
            renderer_draw_box(sprite_batch, x, z, scale, 1.0f, 1.0f, 1.0f);

            char character = grid->data[x + y * grid->width];
            renderer_draw_character(character, sprite_batch, x, z + 1, scale, 0.0f, 0.0f, 0.0f);
            break;
        }
        case GRID_CURSOR_STYLE_UNDERLINE: {
            renderer_draw_underline(sprite_batch, x, z, scale, 1.0f, 1.0f, 1.0f);
            break;
        }
        case GRID_CURSOR_STYLE_BAR: {
            renderer_draw_bar(sprite_batch, x, z, scale, 1.0f, 1.0f, 1.0f);
            break;
        }
    }
}

static void renderer_apply_selection_colors(
    struct Renderer *renderer,
    struct Selection *sorted_selection,
    int32_t x,
    int32_t y,
    struct Color *foreground_color,
    struct Color *background_color
) {

    if (renderer->selection_state != SELECTION_STATE_FINISHED || !selection_contains_point(sorted_selection, x, y)) {
        return;
    }

    struct Color old_background_color = *background_color;
    *background_color = *foreground_color;
    *foreground_color = old_background_color;
}

static void renderer_draw_scrollback(
    struct Renderer *renderer, struct Grid *grid, int32_t visible_scrollback_line_count
) {

    struct Selection sorted_selection = selection_sorted(&renderer->selection);

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

        for (size_t x = 0; x < grid->width; x++) {
            char character = ' ';
            uint32_t background_hex_color = GRID_COLOR_BACKGROUND_DEFAULT;
            uint32_t foreground_hex_color = GRID_COLOR_FOREGROUND_DEFAULT;

            if (x < scrollback_line_length) {
                character = grid->scrollback_lines.data[scrollback_y].data[x];
                background_hex_color = grid->scrollback_lines.data[scrollback_y].background_colors[x];
                foreground_hex_color = grid->scrollback_lines.data[scrollback_y].foreground_colors[x];
            }

            struct Color background_color = color_from_hex(background_hex_color);
            struct Color foreground_color = color_from_hex(foreground_hex_color);

            renderer_apply_selection_colors(
                renderer,
                &sorted_selection,
                x,
                -renderer->scrollback_distance + y,
                &foreground_color,
                &background_color
            );
            renderer_draw_tile(character, foreground_color, background_color, sprite_batch, x, 0, renderer->scale);
        }

        sprite_batch_end(sprite_batch, renderer->texture_atlas.width, renderer->texture_atlas.height);
    }
}

static void renderer_draw_grid(
    struct Renderer *renderer, struct Grid *grid, int32_t visible_scrollback_line_count, bool do_draw_cursor
) {

    struct Selection sorted_selection = selection_sorted(&renderer->selection);

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

            renderer_apply_selection_colors(
                renderer,
                &sorted_selection,
                x,
                grid_y,
                &foreground_color,
                &background_color
            );
            renderer_draw_tile(character, foreground_color, background_color, sprite_batch, x, 0, renderer->scale);
        }

        if (do_draw_cursor && grid->should_show_cursor && grid_y == grid->cursor_y) {
            renderer_draw_cursor(grid, sprite_batch, grid->cursor_x, grid->cursor_y, 2, renderer->scale);
        }

        sprite_batch_end(sprite_batch, renderer->texture_atlas.width, renderer->texture_atlas.height);
    }
}

void renderer_draw(struct Renderer *renderer, struct Grid *grid, int32_t origin_y, struct Window *window) {
    glClearColor(renderer->background_color.r, renderer->background_color.g, renderer->background_color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(renderer->program);
    glUniformMatrix4fv(renderer->projection_matrix_location, 1, GL_FALSE, (const float *)&renderer->projection_matrix);
    glBindTexture(GL_TEXTURE_2D, renderer->texture_atlas.id);

    int32_t visible_scrollback_line_count = renderer_get_visible_scrollback_line_count(renderer);

    renderer_draw_scrollback(renderer, grid, visible_scrollback_line_count);
    renderer_draw_grid(renderer, grid, visible_scrollback_line_count, window->is_focused);

    for (size_t y = 0; y < renderer->sprite_batch_count; y++) {
        struct SpriteBatch *sprite_batch = &renderer->sprite_batches[y];

        float offset_y = origin_y - (y + 1) * FONT_GLYPH_HEIGHT * renderer->scale;
        glUniform1f(renderer->offset_y_location, offset_y);
        sprite_batch_draw(sprite_batch);
    }

    window_swap_buffers(window);
}

void renderer_resize_viewport(struct Renderer *renderer, int32_t width, int32_t height) {
    glViewport(0, 0, width, height);
    renderer->projection_matrix = matrix4_orthographic(0.0f, (float)width, 0.0f, (float)height, -100.0, 100.0);
}

static void renderer_mark_all_sprite_batches_dirty(struct Renderer *renderer) {
    for (size_t i = 0; i < renderer->sprite_batch_count; i++) {
        renderer->are_sprite_batches_dirty[i] = true;
    }

    renderer->needs_redraw = true;
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

    renderer_on_scroll(renderer);

    renderer->scrollback_distance = 0;
    renderer_mark_all_sprite_batches_dirty(renderer);
}

void renderer_scroll_down(struct Renderer *renderer, bool is_scrolling_with_grid) {
    renderer_on_scroll(renderer);

    if (!is_scrolling_with_grid) {
        renderer->scrollback_distance -= 1;
        if (renderer->scrollback_distance < 0) {
            renderer->scrollback_distance = 0;
            return;
        }
    }

    // Rotate the sprite batchs to allow scrolling without updating every batch.
    struct SpriteBatch first_sprite_batch = renderer->sprite_batches[0];
    memmove(
        &renderer->sprite_batches[0],
        &renderer->sprite_batches[1],
        (renderer->sprite_batch_count - 1) * sizeof(struct SpriteBatch)
    );
    renderer->sprite_batches[renderer->sprite_batch_count - 1] = first_sprite_batch;

    memmove(
        renderer->are_sprite_batches_dirty,
        renderer->are_sprite_batches_dirty + 1,
        (renderer->sprite_batch_count - 1) * sizeof(bool)
    );
    // Now the bottom sprite batch is the only one with newly outdated content.
    renderer->are_sprite_batches_dirty[renderer->sprite_batch_count - 1] = true;

    renderer->needs_redraw = true;
}

void renderer_scroll_up(struct Renderer *renderer, struct Grid *grid) {
    renderer_on_scroll(renderer);

    renderer->scrollback_distance += 1;
    if (renderer->scrollback_distance > grid->scrollback_lines.length) {
        renderer->scrollback_distance = grid->scrollback_lines.length;
        return;
    }

    // Rotate the sprite batchs to allow scrolling without updating every batch.
    struct SpriteBatch last_sprite_batch = renderer->sprite_batches[renderer->sprite_batch_count - 1];
    memmove(
        &renderer->sprite_batches[1],
        &renderer->sprite_batches[0],
        (renderer->sprite_batch_count - 1) * sizeof(struct SpriteBatch)
    );
    renderer->sprite_batches[0] = last_sprite_batch;

    memmove(
        renderer->are_sprite_batches_dirty + 1,
        renderer->are_sprite_batches_dirty,
        (renderer->sprite_batch_count - 1) * sizeof(bool)
    );
    // Now the top sprite batch is the only one with newly outdated content.
    renderer->are_sprite_batches_dirty[0] = true;

    renderer->needs_redraw = true;
}

void renderer_destroy(struct Renderer *renderer) {
    for (size_t i = 0; i < renderer->sprite_batch_count; i++) {
        sprite_batch_destroy(&renderer->sprite_batches[i]);
    }

    free(renderer->sprite_batches);
    free(renderer->are_sprite_batches_dirty);

    texture_destroy(&renderer->texture_atlas);
    program_destroy(renderer->program);
}