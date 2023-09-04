#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec3 in_tex_coord;

out vec3 vertex_color;
out vec3 vertex_tex_coord;

uniform mat4 view_matrix;
uniform mat4 projection_matrix;
uniform float time_of_day;

void main() {
    gl_Position = projection_matrix * view_matrix * vec4(in_position, 1.0);

    float shade = in_color.r;
    float sunlight_level = in_color.g;
    float light_level = in_color.b;

    sunlight_level *= time_of_day;

    vertex_color = vec3(max(sunlight_level, light_level) * shade);
    vertex_tex_coord = in_tex_coord;
}