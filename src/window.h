#ifndef WINDOW_H
#define WINDOW_H

#include "detect_leak.h"

// Glad needs to be included before GLFW.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "input.h"
#include "selection.h"

#include <stdbool.h>

struct PseudoConsole;
struct Grid;
struct Renderer;

LIST_DEFINE(uint8_t)
LIST_DEFINE(char)

struct Window {
    GLFWwindow *glfw_window;
    int32_t width;
    int32_t height;
    float scale;
    int32_t refresh_rate;
    bool did_resize;
    bool is_visible;
    bool is_focused;

    struct Input input;
    // TODO: Move to input struct:
    // TODO: Rename to something else, mouse inputs are also sent here.
    struct List_uint8_t typed_chars;
    uint32_t mouse_tile_x;
    uint32_t mouse_tile_y;
    struct List_char copied_chars;

    struct Grid *grid;
    struct Renderer *renderer;
};

struct Window window_create(char *title, int32_t width, int32_t height);
void window_show(struct Window *window);
void window_setup(struct Window *window, struct Grid *grid, struct Renderer *renderer);
void window_update(struct Window *window);
void window_set_title(struct Window *window, char *title);
void window_swap_buffers(struct Window *window);
void window_destroy(struct Window *window);

#endif