#ifndef RESOURCES_H
#define RESOURCES_H

#include "../detect_leak.h"

#include <inttypes.h>
#include <glad/glad.h>

struct Texture {
    uint32_t id;
    int32_t width;
    int32_t height;
};

struct TextureArray {
    uint32_t id;
    int32_t width;
    int32_t height;
    size_t layer_count;
};

uint32_t shader_create(char *file_path, GLenum shader_type);

uint32_t program_create(char *vertex_path, char *fragment_path);
void program_destroy(uint32_t program);

struct Texture texture_create(char *file_path);
void texture_destroy(struct Texture *texture);

#endif