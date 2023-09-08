#ifndef RENDERER_H
#define RENDERER_H

#include "../grid.h"
#include "resources.h"
#include "sprite_batch.h"

#include <cglm/struct.h>

struct Renderer {
    struct SpriteBatch sprite_batch;
    struct Texture texture_atlas;
    mat4s projection_matrix;
    uint32_t program;
    int32_t projection_matrix_location;
};

struct Renderer renderer_create(struct Grid *grid);
void renderer_draw(struct Renderer *renderer, struct Grid *grid, int32_t origin_y, GLFWwindow *glfw_window);
void renderer_resize_viewport(struct Renderer *renderer, int32_t width, int32_t height);
void renderer_resize(struct Renderer *renderer, struct Grid *grid);
void renderer_destroy(struct Renderer *renderer);

#endif