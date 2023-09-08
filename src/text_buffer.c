#include "text_buffer.h"

#include <stdlib.h>
#include <assert.h>

struct TextBuffer text_buffer_create(void) {
    struct TextBuffer text_buffer = (struct TextBuffer){
        .data = malloc(TEXT_BUFFER_CAPACITY * sizeof(char)),
    };
    assert(text_buffer.data);

    return text_buffer;
}

bool text_buffer_match_char(struct TextBuffer *text_buffer, char character, size_t *i) {
    if (*i >= text_buffer->length) {
        return false;
    }

    if (text_buffer->data[*i] != character) {
        return false;
    }

    *i += 1;
    return true;
}

bool text_buffer_digit(struct TextBuffer *text_buffer, size_t i) {
    if (i >= text_buffer->length) {
        return false;
    }

    return iswdigit(text_buffer->data[i]);
}

void text_buffer_destroy(struct TextBuffer *text_buffer) {
    free(text_buffer->data);
}