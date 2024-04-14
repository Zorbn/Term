#include "geometry.h"

struct Matrix4 matrix4_orthographic(float left, float right, float bottom, float top, float near_z, float far_z) {
    float inverse_width = 1.0f / (right - left);
    float inverse_height = 1.0f / (top - bottom);
    float inverse_z = -1.0f / (far_z - near_z);

    return (struct Matrix4){
        .xx = 2.0f * inverse_width,
        .xy = 0.0f,
        .xz = 0.0f,
        .xw = 0.0f,

        .yx = 0.0f,
        .yy = 2.0f * inverse_height,
        .yz = 0.0f,
        .yw = 0.0f,

        .zx = 0.0f,
        .zy = 0.0f,
        .zz = 2.0f * inverse_z,
        .zw = 0.0f,

        .wx = -(right + left) * inverse_width,
        .wy = -(top + bottom) * inverse_height,
        .wz = (far_z + near_z) * inverse_z,
        .ww = 1.0f,
    };
}

int32_t int32_clamp(int32_t value, int32_t min, int32_t max) {
    if (value < min) {
        return min;
    }

    if (value > max) {
        return max;
    }

    return value;
}

int32_t int32_min(int32_t a, int32_t b) {
    if (a < b) {
        return a;
    }

    return b;
}

int32_t int32_max(int32_t a, int32_t b) {
    if (a > b) {
        return a;
    }

    return b;
}