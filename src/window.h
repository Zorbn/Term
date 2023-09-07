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

struct Window {
    GLFWwindow *glfw_window;
    int32_t width;
    int32_t height;
    bool was_resized;
    struct Input input;
    // TODO: Move to input struct:
    struct List_uint8_t typed_chars;
    PseudoConsole console; // TODO: Is this going to stay here? If it is initialize it in window_create.
};

struct Window window_create(char *title, int32_t width, int32_t height);
void window_update(struct Window *window);
void window_destroy(struct Window *window);

#endif