#include "input.h"

struct Input input_create(void) {
    return (struct Input){
        .held_buttons = list_create_int32_t(16),
        .pressed_buttons = list_create_int32_t(16),
        .released_buttons = list_create_int32_t(16),
    };
}

static int32_t input_index_of_button(struct List_int32_t *buttons, int32_t button) {
    for (int32_t i = 0; i < buttons->length; i++) {
        if (buttons->data[i] == button) {
            return i;
        }
    }

    return -1;
}

void input_update_button(struct Input *input, int32_t button, int32_t action) {
    switch (action) {
        case GLFW_REPEAT:
        case GLFW_PRESS: {
            list_push_int32_t(&input->pressed_buttons, button);
            list_push_int32_t(&input->held_buttons, button);

            break;
        }
        case GLFW_RELEASE: {
            list_push_int32_t(&input->released_buttons, button);

            int32_t held_button_i = input_index_of_button(&input->held_buttons, button);

            if (held_button_i != -1) {
                list_remove_unordered_int32_t(&input->held_buttons, held_button_i);
            }

            break;
        }
        default: {
            break;
        }
    }
}

void input_update(struct Input *input) {
    list_reset_int32_t(&input->pressed_buttons);
}

bool input_is_button_held(struct Input *input, int32_t button) {
    return input_index_of_button(&input->held_buttons, button) != -1;
}

bool input_is_button_pressed(struct Input *input, int32_t button) {
    return input_index_of_button(&input->pressed_buttons, button) != -1;
}

bool input_is_button_released(struct Input *input, int32_t button) {
    return input_index_of_button(&input->released_buttons, button) != -1;
}

void input_destroy(struct Input *input) {
    list_destroy_int32_t(&input->held_buttons);
    list_destroy_int32_t(&input->pressed_buttons);
}