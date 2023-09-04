#ifndef SPRITE_BATCH_H
#define SPRITE_BATCH_H

#include "../detect_leak.h"

#include "../list.h"
#include "mesh.h"

struct Sprite {
    float x;
    float y;
    float z;
    float width;
    float height;
    float texture_x;
    float texture_y;
    float texture_width;
    float texture_height;
};

// Define a list of sprites, type names passed to LIST_DEFINE can't have spaces.
typedef struct Sprite struct_Sprite;
LIST_DEFINE(struct_Sprite)

struct SpriteBatch {
    struct List_struct_Sprite sprites;
    struct List_float vertices;
    struct List_uint32_t indices;
    struct Mesh mesh;
};

struct SpriteBatch sprite_batch_create(int capacity);
void sprite_batch_begin(struct SpriteBatch *sprite_batch);
void sprite_batch_add(struct SpriteBatch *sprite_batch, struct Sprite sprite);
void sprite_batch_end(struct SpriteBatch *sprite_batch, int32_t texture_atlas_width, int32_t texture_atlas_height);
void sprite_batch_draw(struct SpriteBatch *sprite_batch);
void sprite_batch_destroy(struct SpriteBatch *sprite_batch);

#endif