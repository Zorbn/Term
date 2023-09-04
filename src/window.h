#ifndef WINDOW_H
#define WINDOW_H

#include "detect_leak.h"

// Glad needs to be included before GLFW.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "input.h"

#include <stdbool.h>

struct Window {
    GLFWwindow *glfw_window;
    int32_t width;
    int32_t height;
    bool was_resized;
    bool is_mouse_locked;
    double last_mouse_x;
    double last_mouse_y;
    bool is_mouse_up_to_date;
    struct Input input;
};

struct Window window_create(char *title, int32_t width, int32_t height);
void window_update_mouse_lock(struct Window *window);
void window_get_mouse_delta(struct Window *window, float *delta_x, float *delta_y);
void window_update(struct Window *window);
void window_destroy(struct Window *window);

#endif