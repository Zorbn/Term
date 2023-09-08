#ifndef WINDOW_H
#define WINDOW_H

#include "detect_leak.h"

// Glad needs to be included before GLFW.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "input.h"
#include "pseudo_console.h"

#include <stdbool.h>

LIST_DEFINE(uint8_t)

struct Grid;
struct Renderer;
void renderer_draw(struct Renderer *renderer, struct Grid *grid, int32_t origin_y, GLFWwindow *glfw_window);
void renderer_resize_viewport(struct Renderer *renderer, int32_t width, int32_t height);

struct Window {
    GLFWwindow *glfw_window;
    int32_t width;
    int32_t height;
    bool did_resize;
    struct Input input;
    // TODO: Move to input struct:
    struct List_uint8_t typed_chars;
    struct PseudoConsole pseudo_console; // TODO: Is this going to stay here? If it is initialize it in window_create.
    struct Grid *grid;
    struct Renderer *renderer;
};

struct Window window_create(char *title, int32_t width, int32_t height);
void window_setup_resize_callback(struct Window *window, struct Grid *grid, struct Renderer *renderer);
void window_update(struct Window *window);
void window_destroy(struct Window *window);

#endif