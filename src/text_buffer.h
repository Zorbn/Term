#ifndef DATA_H
#define DATA_H

#include <stdbool.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define TEXT_BUFFER_CAPACITY 8192

struct TextBuffer {
    char *data;
    DWORD length;
    // The number of characters kept from the last read.
    size_t kept_length;
};

struct TextBuffer text_buffer_create(void);
bool text_buffer_match_char(struct TextBuffer *text_buffer, char character, size_t *i);
bool text_buffer_digit(struct TextBuffer *text_buffer, size_t i);
void text_buffer_keep_from_i(struct TextBuffer *text_buffer, size_t i);
void text_buffer_destroy(struct TextBuffer *text_buffer);

#endif