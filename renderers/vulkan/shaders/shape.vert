#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 a;
layout(location = 3) in vec2 b;
layout(location = 4) in vec2 c;
layout(location = 5) in uint is_triangle;
layout(location = 0) flat out vec4 fragment_color;
layout(location = 1) flat out vec2 triangle_a;
layout(location = 2) flat out vec2 triangle_b;
layout(location = 3) flat out vec2 triangle_c;
layout(location = 4) flat out uint triangle_kind;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    fragment_color = color;
    triangle_a = a;
    triangle_b = b;
    triangle_c = c;
    triangle_kind = is_triangle;
}
