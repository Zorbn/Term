#ifndef GEOMETRY_H
#define GEOMETRY_H

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

#endif