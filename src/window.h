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
    struct Input input;
};

struct Window window_create(char *title, int32_t width, int32_t height);
void window_update(struct Window *window);
void window_destroy(struct Window *window);

#endif