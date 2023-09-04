#include "input.h"

struct Input input_create(void) {
    return (struct Input){
        .held_buttons = list_create_int32_t(16),
        .pressed_buttons = list_create_int32_t(16),
    };
}

void input_update_button(struct Input *input, int32_t button, int32_t action) {
    switch (action) {
        case GLFW_PRESS:
            bool was_button_held = false;
            for (size_t i = 0; i < input->held_buttons.length; i++) {
                if (input->held_buttons.data[i] == button) {
                    was_button_held = true;
                    break;
                }
            }

            if (!was_button_held) {
                list_push_int32_t(&input->pressed_buttons, button);
            }

            list_push_int32_t(&input->held_buttons, button);

            break;
        case GLFW_RELEASE:
            for (size_t i = 0; i < input->held_buttons.length; i++) {
                if (input->held_buttons.data[i] != button) {
                    continue;
                }

                list_remove_unordered_int32_t(&input->held_buttons, i);

                break;
            }
        default:
            break;
    }
}

void input_update(struct Input *input) {
    list_reset_int32_t(&input->pressed_buttons);
}

bool input_is_button_held(struct Input *input, int32_t button) {
    for (size_t i = 0; i < input->held_buttons.length; i++) {
        if (input->held_buttons.data[i] == button) {
            return true;
        }
    }

    return false;
}

bool input_is_button_pressed(struct Input *input, int32_t button) {
    for (size_t i = 0; i < input->pressed_buttons.length; i++) {
        if (input->pressed_buttons.data[i] == button) {
            return true;
        }
    }

    return false;
}

void input_destroy(struct Input *input) {
    list_destroy_int32_t(&input->held_buttons);
    list_destroy_int32_t(&input->pressed_buttons);
}