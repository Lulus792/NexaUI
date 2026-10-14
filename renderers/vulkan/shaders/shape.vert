#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 a;
layout(location = 3) in vec2 b;
layout(location = 4) in vec2 c;
layout(location = 5) in uint shape_kind_input;
layout(location = 0) flat out vec4 fragment_color;
layout(location = 1) flat out vec2 shape_a;
layout(location = 2) flat out vec2 shape_b;
layout(location = 3) flat out vec2 shape_c;
layout(location = 4) flat out uint shape_kind;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    fragment_color = color;
    shape_a = a;
    shape_b = b;
    shape_c = c;
    shape_kind = shape_kind_input;
}
