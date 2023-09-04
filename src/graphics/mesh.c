#include "mesh.h"

const size_t vertex_component_count = 9;

struct Mesh mesh_create(const float *vertices, uint32_t vertex_count, const uint32_t *indices, uint32_t index_count) {
    uint32_t vbo;
    glGenBuffers(1, &vbo);

    uint32_t vao;
    glGenVertexArrays(1, &vao);

    glBindVertexArray(vao);

    const uint64_t sizeof_vec3 = sizeof(float) * 3;
    const uint64_t sizeof_vertex = sizeof_vec3 * 3;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof_vertex * vertex_count, vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof_vertex, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof_vertex, (void *)sizeof_vec3);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof_vertex, (void *)(sizeof_vec3 * 2));
    glEnableVertexAttribArray(2);

    uint32_t ebo;
    glGenBuffers(1, &ebo);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * index_count, indices, GL_STATIC_DRAW);

    return (struct Mesh){
        .vao = vao,
        .vbo = vbo,
        .ebo = ebo,
        .index_count = index_count,
    };
}

void mesh_draw(struct Mesh *mesh) {
    if (mesh->index_count == 0) {
        return;
    }

    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glDrawElements(GL_TRIANGLES, mesh->index_count, GL_UNSIGNED_INT, 0);
}

void mesh_destroy(struct Mesh *mesh) {
    if (mesh->index_count == 0) {
        return;
    }

    glDeleteBuffers(1, &mesh->vbo);
    glDeleteBuffers(1, &mesh->ebo);
    glDeleteVertexArrays(1, &mesh->vao);
}