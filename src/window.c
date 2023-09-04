#include "window.h"

#include <stdio.h>
#include <stdlib.h>

void framebuffer_size_callback(GLFWwindow *glfw_window, int32_t width, int32_t height) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);
    window->width = width;
    window->height = height;
    window->was_resized = true;
}

void key_callback(GLFWwindow *glfw_window, int32_t key, int32_t scancode, int32_t action, int32_t mods) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);
    input_update_button(&window->input, key, action);
}

void mouse_button_callback(GLFWwindow *glfw_window, int32_t button, int32_t action, int32_t mods) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);
    input_update_button(&window->input, button, action);
}

struct Window window_create(char *title, int32_t width, int32_t height) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *glfw_window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!glfw_window) {
        puts("Failed to create GLFW window");
        glfwTerminate();
        exit(-1);
    }
    glfwMakeContextCurrent(glfw_window);
    glfwSwapInterval(0);

    struct Window window = {
        .glfw_window = glfw_window,
        .is_mouse_locked = false,
        .is_mouse_up_to_date = false,
        .input = input_create(),
    };
    glfwSetWindowUserPointer(glfw_window, (void *)&window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        puts("Failed to initialize GLAD");
        exit(-1);
    }

    glfwSetFramebufferSizeCallback(glfw_window, framebuffer_size_callback);
    framebuffer_size_callback(glfw_window, width, height);
    glfwSetKeyCallback(glfw_window, key_callback);
    glfwSetMouseButtonCallback(glfw_window, mouse_button_callback);

    return window;
}

void window_update_mouse_lock(struct Window *window) {
    if (!window->is_mouse_locked && glfwGetMouseButton(window->glfw_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        glfwSetInputMode(window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        window->is_mouse_locked = true;
        window->is_mouse_up_to_date = false;
    } else if (window->is_mouse_locked && glfwGetKey(window->glfw_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetInputMode(window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        window->is_mouse_locked = false;
    }
}

void window_get_mouse_delta(struct Window *window, float *delta_x, float *delta_y) {
    if (!window->is_mouse_locked) {
        *delta_x = 0.0f;
        *delta_y = 0.0f;
        return;
    }

    double mouse_x, mouse_y;
    glfwGetCursorPos(window->glfw_window, &mouse_x, &mouse_y);

    if (!window->is_mouse_up_to_date) {
        window->last_mouse_x = mouse_x;
        window->last_mouse_y = mouse_y;
        window->is_mouse_up_to_date = true;
        *delta_x = 0.0f;
        *delta_y = 0.0f;
        return;
    }

    *delta_x = (float)(mouse_x - window->last_mouse_x);
    *delta_y = (float)(mouse_y - window->last_mouse_y);
    window->last_mouse_x = mouse_x;
    window->last_mouse_y = mouse_y;
}

void window_update(struct Window *window) {
    window_update_mouse_lock(window);
    input_update(&window->input);
}

void window_destroy(struct Window *window) {
    input_destroy(&window->input);

    glfwTerminate();
}