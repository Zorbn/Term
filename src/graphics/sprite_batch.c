#include "sprite_batch.h"
#include "../geometry.h"

#define SPRITE_TEXTURE_PADDING 0.01f

const struct Vector3 sprite_vertices[4] = {
    {0, 0, 0},
    {0, 1, 0},
    {1, 1, 0},
    {1, 0, 0},
};

const struct Vector2 sprite_uvs[4] = {
    {0, 1},
    {0, 0},
    {1, 0},
    {1, 1},
};

const uint32_t sprite_indices[] = {0, 2, 1, 0, 3, 2};

struct SpriteBatch sprite_batch_create(int capacity) {
    return (struct SpriteBatch){
        .sprites = list_create_struct_Sprite(capacity),
        .vertices = list_create_float(capacity * 4 * vertex_component_count),
        .indices = list_create_uint32_t(capacity * 6),
        .mesh = mesh_create(capacity * 4, capacity * 6),
    };
}

void sprite_batch_begin(struct SpriteBatch *sprite_batch) {
    list_reset_struct_Sprite(&sprite_batch->sprites);
}

void sprite_batch_add(struct SpriteBatch *sprite_batch, struct Sprite sprite) {
    list_push_struct_Sprite(&sprite_batch->sprites, sprite);
}

void sprite_batch_end(struct SpriteBatch *sprite_batch, int32_t texture_atlas_width, int32_t texture_atlas_height) {
    list_reset_float(&sprite_batch->vertices);
    list_reset_uint32_t(&sprite_batch->indices);

    const float inv_texture_width = 1.0f / texture_atlas_width;
    const float inv_texture_height = 1.0f / texture_atlas_height;

    for (size_t i = 0; i < sprite_batch->sprites.length; i++) {
        struct Sprite *sprite = &sprite_batch->sprites.data[i];

        uint32_t vertex_count = sprite_batch->vertices.length / vertex_component_count;

        for (size_t index_i = 0; index_i < 6; index_i++) {
            uint32_t index = vertex_count + sprite_indices[index_i];
            list_push_uint32_t(&sprite_batch->indices, index);
        }

        for (size_t vertex_i = 0; vertex_i < 4; vertex_i++) {
            // Position:
            float vertex_x = sprite->x + sprite_vertices[vertex_i].x * sprite->width;
            float vertex_y = sprite->y + sprite_vertices[vertex_i].y * sprite->height;
            float vertex_z = sprite->z + sprite_vertices[vertex_i].z;
            list_push_float(&sprite_batch->vertices, vertex_x);
            list_push_float(&sprite_batch->vertices, vertex_y);
            list_push_float(&sprite_batch->vertices, vertex_z);

            // Color:
            list_push_float(&sprite_batch->vertices, sprite->r);
            list_push_float(&sprite_batch->vertices, sprite->g);
            list_push_float(&sprite_batch->vertices, sprite->b);

            // UV:
            float u = sprite->texture_x + SPRITE_TEXTURE_PADDING +
                      sprite_uvs[vertex_i].x * (sprite->texture_width - SPRITE_TEXTURE_PADDING);
            float v = sprite->texture_y + SPRITE_TEXTURE_PADDING +
                      sprite_uvs[vertex_i].y * (sprite->texture_height - SPRITE_TEXTURE_PADDING);
            list_push_float(&sprite_batch->vertices, u * inv_texture_width);
            list_push_float(&sprite_batch->vertices, v * inv_texture_height);
            list_push_float(&sprite_batch->vertices, 0.0f);
        }
    }

    mesh_update(
        &sprite_batch->mesh,
        sprite_batch->vertices.data,
        sprite_batch->vertices.length / vertex_component_count,
        sprite_batch->indices.data,
        sprite_batch->indices.length
    );
}

void sprite_batch_draw(struct SpriteBatch *sprite_batch) {
    mesh_draw(&sprite_batch->mesh);
}

void sprite_batch_destroy(struct SpriteBatch *sprite_batch) {
    mesh_destroy(&sprite_batch->mesh);
    list_destroy_struct_Sprite(&sprite_batch->sprites);
    list_destroy_float(&sprite_batch->vertices);
    list_destroy_uint32_t(&sprite_batch->indices);
}