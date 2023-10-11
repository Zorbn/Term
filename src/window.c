#include "window.h"

#include "font.h"

#include <stdio.h>
#include <stdlib.h>

const float min_zoom_level = 1;
const float max_zoom_level = 4;

void framebuffer_size_callback(GLFWwindow *glfw_window, int32_t width, int32_t height) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);
    window->width = width;
    window->height = height;
    window->did_resize = true;

    renderer_resize_viewport(window->renderer, width, height);
    renderer_draw(window->renderer, window->grid, height, glfw_window);
}

void key_callback(GLFWwindow *glfw_window, int32_t key, int32_t scancode, int32_t action, int32_t mods) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);
    input_update_button(&window->input, key, action);

    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }

    uint8_t write_char = 0;
    bool needs_write = true;
    bool is_key_arrow = false;
    bool is_key_cursor = false;

    switch (key) {
        case GLFW_KEY_ENTER: {
            write_char = '\r';
            break;
        }
        case GLFW_KEY_ESCAPE: {
            write_char = '\x1b';
            break;
        }
        case GLFW_KEY_BACKSPACE: {
            write_char = '\x7f';
            break;
        }
        case GLFW_KEY_TAB: {
            write_char = '\t';
            break;
        }
        case GLFW_KEY_UP: {
            is_key_arrow = true;
            write_char = 'A';
            break;
        }
        case GLFW_KEY_DOWN: {
            is_key_arrow = true;
            write_char = 'B';
            break;
        }
        case GLFW_KEY_RIGHT: {
            is_key_arrow = true;
            write_char = 'C';
            break;
        }
        case GLFW_KEY_LEFT: {
            is_key_arrow = true;
            write_char = 'D';
            break;
        }
        case GLFW_KEY_HOME: {
            is_key_cursor = true;
            write_char = 'H';
            break;
        }
        case GLFW_KEY_END: {
            is_key_cursor = true;
            write_char = 'F';
            break;
        }
        default: {
            needs_write = false;
            break;
        }
    }

    is_key_cursor = is_key_cursor || is_key_arrow;

    bool is_shift_pressed = mods & GLFW_MOD_SHIFT;
    bool is_ctrl_pressed = mods & GLFW_MOD_CONTROL;
    bool is_alt_pressed = mods & GLFW_MOD_ALT;

    if (needs_write) {
        if (is_key_cursor) {
            list_push_uint8_t(&window->typed_chars, '\x1b');
            list_push_uint8_t(&window->typed_chars, '[');
        }

        if (is_key_arrow) {
            if (mods) {
                list_push_uint8_t(&window->typed_chars, '1');
                list_push_uint8_t(&window->typed_chars, ';');
            }

            if (is_shift_pressed && is_ctrl_pressed) {
                list_push_uint8_t(&window->typed_chars, '6');
            } else if (is_shift_pressed && is_alt_pressed) {
                list_push_uint8_t(&window->typed_chars, '4');
            } else if (is_shift_pressed) {
                list_push_uint8_t(&window->typed_chars, '2');
            } else if (is_ctrl_pressed) {
                list_push_uint8_t(&window->typed_chars, '5');
            }
        }

        list_push_uint8_t(&window->typed_chars, write_char);
        return;
    }

    // Handle ctrl + [key] and alt + [key] here because those won't be send to character_callback.
    if (!is_ctrl_pressed && !is_alt_pressed) {
        return;
    }

    const char *key_name = glfwGetKeyName(key, scancode);
    if (!key_name || strlen(key_name) != 1) {
        return;
    }

    uint8_t key_char = key_name[0];

    if (is_ctrl_pressed) {
        bool block_key = true;
        switch (key_char) {
            case '-': {
                if (is_shift_pressed || window->scale <= min_zoom_level) {
                    break;
                }

                window->scale--;
                window->did_resize = true;
                break;
            }
            case '=': {
                if (!is_shift_pressed || window->scale >= max_zoom_level) {
                    break;
                }

                window->scale++;
                window->did_resize = true;
                break;
            }
            default: {
                block_key = false;
                break;
            }
        }

        if (block_key) {
            return;
        }

        key_char &= 0x1f;
    }

    if (is_alt_pressed) {
        list_push_uint8_t(&window->typed_chars, '\x1b');
    }

    list_push_uint8_t(&window->typed_chars, key_char);
}

void list_push_digits(struct List_uint8_t *list, uint32_t x) {
    if (x == 0) {
        list_push_uint8_t(list, '0');
        return;
    }

    char digits[10];
    size_t digit_count = 0;

    while (x > 0) {
        digits[digit_count] = '0' + x % 10;
        x /= 10;
        digit_count++;
    }

    for (int32_t i = digit_count - 1; i >= 0; i--) {
        list_push_uint8_t(list, digits[i]);
    }
}

void send_mouse_input_sgr(struct Window *window, uint8_t encoded_button, int32_t action, int32_t mods) {
    list_push_uint8_t(&window->typed_chars, '\x1b');
    list_push_uint8_t(&window->typed_chars, '[');
    list_push_uint8_t(&window->typed_chars, '<');

    list_push_digits(&window->typed_chars, encoded_button);
    list_push_uint8_t(&window->typed_chars, ';');

    list_push_digits(&window->typed_chars, window->mouse_tile_x);
    list_push_uint8_t(&window->typed_chars, ';');
    list_push_digits(&window->typed_chars, window->mouse_tile_y);

    list_push_uint8_t(&window->typed_chars, action == GLFW_RELEASE ? 'm' : 'M');
}

void send_mouse_input_normal(struct Window *window, uint8_t encoded_button, int32_t action, int32_t mods) {
    list_push_uint8_t(&window->typed_chars, '\x1b');
    list_push_uint8_t(&window->typed_chars, '[');
    list_push_uint8_t(&window->typed_chars, 'M');

    list_push_uint8_t(&window->typed_chars, encoded_button + 32);
    list_push_uint8_t(&window->typed_chars, ';');

    list_push_uint8_t(&window->typed_chars, window->mouse_tile_x + 32);
    list_push_uint8_t(&window->typed_chars, window->mouse_tile_y + 32);
}

void send_mouse_input(struct Window *window, int32_t button, int32_t action, int32_t mods, bool is_motion) {
    uint8_t encoded_button = button;
    if (mods & GLFW_MOD_SHIFT) {
        encoded_button += 4;
    }
    if (mods & GLFW_MOD_ALT) {
        encoded_button += 8;
    }
    if (mods & GLFW_MOD_CONTROL) {
        encoded_button += 16;
    }
    if (is_motion) {
        encoded_button += 32;
    }

    if (window->grid->should_use_sgr_format) {
        send_mouse_input_sgr(window, encoded_button, action, mods);
        return;
    }

    send_mouse_input_normal(window, encoded_button, action, mods);
}

void mouse_button_callback(GLFWwindow *glfw_window, int32_t button, int32_t action, int32_t mods) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);
    input_update_button(&window->input, button, action);

    if (grid_get_mouse_mode(window->grid) == GRID_MOUSE_MODE_NONE || button < 0 || button > 2) {
        return;
    }

    send_mouse_input(window, button, action, mods, false);
}

void mouse_move_callback(GLFWwindow *glfw_window, double mouse_x, double mouse_y) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);

    uint32_t mouse_tile_x = mouse_x / (FONT_GLYPH_WIDTH * window->scale) + 1;
    uint32_t mouse_tile_y = mouse_y / (FONT_GLYPH_HEIGHT * window->scale) + 1;

    if (mouse_tile_x == window->mouse_tile_x && mouse_tile_y == window->mouse_tile_y) {
        return;
    }

    window->mouse_tile_x = mouse_tile_x;
    window->mouse_tile_y = mouse_tile_y;

    enum GridMouseMode mouse_mode = grid_get_mouse_mode(window->grid);

    if (mouse_mode == GRID_MOUSE_MODE_NONE || mouse_mode == GRID_MOUSE_MODE_BUTTON) {
        return;
    }

    int32_t button = 3;
    if (input_is_button_held(&window->input, GLFW_MOUSE_BUTTON_LEFT)) {
        button = 0;
    } else if (input_is_button_held(&window->input, GLFW_MOUSE_BUTTON_RIGHT)) {
        button = 1;
    } else if (input_is_button_held(&window->input, GLFW_MOUSE_BUTTON_MIDDLE)) {
        button = 2;
    }

    if (button == 3 && mouse_mode != GRID_MOUSE_MODE_ANY) {
        return;
    }

    send_mouse_input(window, button, GLFW_PRESS, 0, true);
}

void mouse_scroll_callback(GLFWwindow *glfw_window, double scroll_x, double scroll_y) {
    struct Window *window = glfwGetWindowUserPointer(glfw_window);

    if (grid_get_mouse_mode(window->grid) == GRID_MOUSE_MODE_NONE) {
        return;
    }

    if (scroll_y < 0) {
        send_mouse_input(window, 65, GLFW_PRESS, 0, false);
    } else if (scroll_y > 0) {
        send_mouse_input(window, 64, GLFW_PRESS, 0, false);
    }
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
        .width = width,
        .height = height,
        .scale = 1.0f,
    };
    glfwSetWindowUserPointer(glfw_window, (void *)&window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        puts("Failed to initialize GLAD");
        exit(-1);
    }

    glfwSetKeyCallback(glfw_window, key_callback);
    glfwSetCharCallback(glfw_window, character_callback);

    return window;
}

void window_setup(struct Window *window, struct Grid *grid, struct Renderer *renderer) {
    window->grid = grid;
    window->renderer = renderer;
    glfwSetFramebufferSizeCallback(window->glfw_window, framebuffer_size_callback);
    framebuffer_size_callback(window->glfw_window, window->width, window->height);
    glfwSetMouseButtonCallback(window->glfw_window, mouse_button_callback);
    glfwSetCursorPosCallback(window->glfw_window, mouse_move_callback);
    glfwSetScrollCallback(window->glfw_window, mouse_scroll_callback);
}

void window_update(struct Window *window) {
    input_update(&window->input);
    list_reset_uint8_t(&window->typed_chars);
}

void window_set_title(struct Window *window, char *title) {
    glfwSetWindowTitle(window->glfw_window, title);
}

void window_destroy(struct Window *window) {
    input_destroy(&window->input);
    list_destroy_uint8_t(&window->typed_chars);

    glfwTerminate();
}