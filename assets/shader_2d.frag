#version 330 core

in vec3 vertex_color;
in vec3 vertex_tex_coord;

out vec4 out_frag_color;

uniform sampler2D texture_sampler;

void main() {
    vec4 texture_color = texture(texture_sampler, vertex_tex_coord.xy);
    if (texture_color.a < 1.0) {
        discard;
    }

    out_frag_color = vec4(vertex_color, 1.0f) * texture_color;
}