#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../grid.h"
#include "../color.h"
#include "../geometry.h"
#include "../window.h"
#include "resources.h"
#include "sprite_batch.h"

struct Renderer {
    struct SpriteBatch *sprite_batches;
    bool *are_sprite_batches_dirty;
    size_t sprite_batch_count;

    float scale;
    struct Color background_color;

    struct Texture texture_atlas;
    struct Matrix4 projection_matrix;
    uint32_t program;
    int32_t projection_matrix_location;
    int32_t offset_y_location;

    int32_t scrollback_distance;

    _Atomic(bool) needs_redraw;
};

struct Renderer renderer_create(size_t width, size_t height);
void renderer_on_row_changed(void *context, int32_t y);
void renderer_on_push_scrollback_line(void *context);
void renderer_draw(struct Renderer *renderer, struct Grid *grid, int32_t origin_y, struct Window *window);
void renderer_resize_viewport(struct Renderer *renderer, int32_t width, int32_t height);
void renderer_resize(struct Renderer *renderer, size_t width, size_t height, float scale);
void renderer_scroll_reset(struct Renderer *renderer);
void renderer_scroll_down(struct Renderer *renderer, bool is_scrolling_with_grid);
void renderer_scroll_up(struct Renderer *renderer, struct Grid *grid);
void renderer_destroy(struct Renderer *renderer);

#endif