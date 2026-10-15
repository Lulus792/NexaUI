#version 450

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 a;
layout(location = 3) in vec2 b;
layout(location = 4) in vec2 c;
layout(location = 5) in uint shape_kind_input;
layout(location = 6) in vec2 source_size;
layout(location = 7) in uvec4 instance_clip;
layout(location = 0) flat out vec4 fragment_color;
layout(location = 1) flat out vec2 shape_a;
layout(location = 2) flat out vec2 shape_b;
layout(location = 3) flat out vec2 shape_c;
layout(location = 4) flat out uint shape_kind;
layout(location = 5) flat out vec2 bitmap_size;
layout(location = 6) flat out uvec4 pixel_clip;

void main() {
    bool right = gl_VertexIndex == 1 || gl_VertexIndex == 3 || gl_VertexIndex == 4;
    bool bottom = gl_VertexIndex == 2 || gl_VertexIndex == 4 || gl_VertexIndex == 5;
    gl_Position = vec4(right ? position.z : position.x, bottom ? position.w : position.y, 0.0, 1.0);
    fragment_color = color;
    shape_a = a;
    shape_b = b;
    shape_c = c;
    shape_kind = shape_kind_input;
    bitmap_size = source_size;
    pixel_clip = instance_clip;
}
