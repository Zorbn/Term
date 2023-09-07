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

    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }

    bool is_ctrl_pressed = mods & GLFW_MOD_CONTROL;
    bool is_alt_pressed = mods & GLFW_MOD_ALT;

    uint8_t write_char = 0;
    bool needs_write = true;
    bool is_key_arrow = false;

    switch (key) {
        case GLFW_KEY_ENTER:
            write_char = '\r';
            break;
        case GLFW_KEY_ESCAPE:
            write_char = '\x1b';
            break;
        case GLFW_KEY_BACKSPACE:
            write_char = '\x7f';
            break;
        case GLFW_KEY_TAB:
            write_char = '\t';
            break;
        case GLFW_KEY_UP:
            is_key_arrow = true;
            write_char = 'A';
            break;
        case GLFW_KEY_DOWN:
            is_key_arrow = true;
            write_char = 'B';
            break;
        case GLFW_KEY_RIGHT:
            is_key_arrow = true;
            write_char = 'C';
            break;
        case GLFW_KEY_LEFT:
            is_key_arrow = true;
            write_char = 'D';
            break;
        default:
            needs_write = false;
    }


    if (needs_write) {
        if (is_key_arrow) {
            list_push_uint8_t(&window->typed_chars, '\x1b');
            list_push_uint8_t(&window->typed_chars, '[');

            if (is_ctrl_pressed) {
                list_push_uint8_t(&window->typed_chars, '1');
                list_push_uint8_t(&window->typed_chars, ';');
                list_push_uint8_t(&window->typed_chars, '5');
            }
        }

        list_push_uint8_t(&window->typed_chars, write_char);
        return;
    }

    // Handle ctrl + [key] and alt + [key] here because those won't be send to character_callback.
    // TODO: Handle special codes like ctrl + space, ctrl + arrows.
    if (!is_ctrl_pressed && !is_alt_pressed) {
        return;
    }

    const char *key_name = glfwGetKeyName(key, scancode);
    if (!key_name || strlen(key_name) != 1) {
        return;
    }

    uint8_t key_char = key_name[0];

    if (is_ctrl_pressed) {
        key_char &= 0x1f;
    }

    if (is_alt_pressed) {
        list_push_uint8_t(&window->typed_chars, '\x1b');
    }

    list_push_uint8_t(&window->typed_chars, key_char);
}

void mouse_button_callback(GLFWwindow *glfw_window, int32_t button, int32_t action, int32_t mods) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);
    input_update_button(&window->input, button, action);
}

void character_callback(GLFWwindow *glfw_window, uint32_t codepoint) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);
    list_push_uint8_t(&window->typed_chars, (uint8_t)codepoint);
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
        .input = input_create(),
        .typed_chars = list_create_uint8_t(16),
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
    glfwSetCharCallback(glfw_window, character_callback);

    return window;
}

void window_update(struct Window *window) {
    input_update(&window->input);
    list_reset_uint8_t(&window->typed_chars);
}

void window_destroy(struct Window *window) {
    input_destroy(&window->input);
    list_destroy_uint8_t(&window->typed_chars);

    glfwTerminate();
}