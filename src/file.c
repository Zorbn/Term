#include "file.h"

#include <stdio.h>
#include <stdlib.h>

char *get_file_string(char *file_path) {
    FILE *file;
    fopen_s(&file, file_path, "rb");

    if (!file) {
        printf("Failed to open file: %s\n", file_path);
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    long file_length = ftell(file);
    rewind(file);

    if (file_length == -1) {
        printf("Couldn't get file length: %s\n", file_path);
    }

    size_t string_length = (size_t)file_length;
    char *buffer = malloc(string_length + 1);
    if (!buffer) {
        printf("Failed to allocate space for file: %s\n", file_path);
    }

    fread(buffer, 1, string_length, file);
    buffer[string_length] = '\0';

    return buffer;
}