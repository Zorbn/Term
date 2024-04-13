#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <inttypes.h>

struct Vector3
{
    float x;
    float y;
    float z;
};

struct Vector2
{
    float x;
    float y;
};

struct Matrix4
{
    float xx;
    float xy;
    float xz;
    float xw;

    float yx;
    float yy;
    float yz;
    float yw;

    float zx;
    float zy;
    float zz;
    float zw;

    float wx;
    float wy;
    float wz;
    float ww;
};

struct Matrix4 matrix4_orthographic(float left, float right, float bottom, float top, float near_z, float far_z);

int32_t int32_clamp(int32_t value, int32_t min, int32_t max);
int32_t int32_min(int32_t a, int32_t b);
int32_t int32_max(int32_t a, int32_t b);

#endif