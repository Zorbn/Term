#ifndef DATA_H
#define DATA_H

#include <stdbool.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// TODO: Is this too big for the buffer? Is there a way to make it smaller while not splitting escape codes and failing
// to process them?
#define TEXT_BUFFER_CAPACITY 8192

struct TextBuffer {
    char *data;
    DWORD length;
};

struct TextBuffer text_buffer_create(void);
bool text_buffer_match_char(struct TextBuffer *text_buffer, char character, size_t *i);
bool text_buffer_digit(struct TextBuffer *text_buffer, size_t i);
void text_buffer_destroy(struct TextBuffer *text_buffer);

#endif