#include "resources.h"

#include "../file.h"

#include <stdio.h>
#include <stdlib.h>

#include <stb_image/stb_image.h>

uint32_t shader_create(char *file_path, GLenum shader_type) {
    uint32_t shader = glCreateShader(shader_type);

    char *shader_source = get_file_string(file_path);
    glShaderSource(shader, 1, (const char *const *)&shader_source, NULL);
    glCompileShader(shader);

    int32_t success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        printf("Failed to compile shader (%s):\n%s\n", file_path, info_log);
    }

    free(shader_source);

    return shader;
}

uint32_t program_create(char *vertex_path, char *fragment_path) {
    uint32_t vertex_shader = shader_create(vertex_path, GL_VERTEX_SHADER);
    uint32_t fragment_shader = shader_create(fragment_path, GL_FRAGMENT_SHADER);

    uint32_t program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    int32_t success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        printf("Failed to link program of (%s) and (%s):\n%s\n", vertex_path, fragment_path, info_log);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

struct Texture texture_create(char *file_path) {
    uint32_t texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, channel_count;
    uint8_t *data = stbi_load(file_path, &width, &height, &channel_count, 0);
    if (!data) {
        printf("Failed to load texture: %s\n", file_path);
        exit(-1);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    return (struct Texture){
        .id = texture,
        .width = width,
        .height = height,
    };
}

struct TextureArray texture_array_create(char **file_paths, size_t file_count, int32_t width, int32_t height) {
    uint32_t texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    for (size_t i = 0; i < file_count; i++) {
        int texture_width, texture_height, channel_count;
        uint8_t *data = stbi_load(file_paths[i], &texture_width, &texture_height, &channel_count, 4);
        if (!data) {
            printf("Failed to load texture: %s\n", file_paths[i]);
            exit(-1);
        } else if (texture_width != width || texture_height != height) {
            printf("Texture is wrong size for array: %s\n", file_paths[i]);
            exit(-1);
        }

        if (i == 0) {
            glTexImage3D(
                GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, width, height, file_count, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        }

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);
    }

    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    return (struct TextureArray){
        .id = texture,
        .width = width,
        .height = height,
        .layer_count = file_count,
    };
}