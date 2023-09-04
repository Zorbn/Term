#ifndef INPUT_H
#define INPUT_H

#include "detect_leak.h"

#include "list.h"

#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <inttypes.h>

struct Input {
    struct List_int32_t held_buttons;
    struct List_int32_t pressed_buttons;
};

struct Input input_create(void);
void input_update_button(struct Input *input, int32_t button, int32_t action);
void input_update(struct Input *input);
bool input_is_button_held(struct Input *input, int32_t button);
bool input_is_button_pressed(struct Input *input, int32_t button);
void input_destroy(struct Input *input);

#endif