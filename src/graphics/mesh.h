#ifndef MESH_H
#define MESH_H

#include "../detect_leak.h"

#include <glad/glad.h>

#include <inttypes.h>

extern const size_t vertex_component_count;

struct Mesh {
    uint32_t vbo;
    uint32_t vao;
    uint32_t ebo;
    uint32_t index_count;
};

struct Mesh mesh_create(const float *vertices, uint32_t vertex_count, const uint32_t *indices, uint32_t index_count);
void mesh_draw(struct Mesh *mesh);
void mesh_destroy(struct Mesh *mesh);

#endif