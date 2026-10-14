#version 450

layout(location = 0) flat in vec4 fragment_color;
layout(location = 1) flat in vec2 triangle_a;
layout(location = 2) flat in vec2 triangle_b;
layout(location = 3) flat in vec2 triangle_c;
layout(location = 4) flat in uint triangle_kind;
layout(location = 0) out vec4 output_color;
layout(set = 0, binding = 0) uniform sampler2D previous_color;

// Keep the rounding remainder when two large edge products nearly cancel.
// This needs only Vulkan 1.0 float32 operations, not shaderFloat64.
vec2 difference(float a, float b) {
    precise float high = a - b;
    precise float bv = a - high;
    precise float av = high + bv;
    precise float br = bv - b;
    precise float ar = a - av;
    precise float low = ar + br;
    return vec2(high, low);
}

vec2 product(vec2 a, vec2 b) {
    precise float high = a.x * b.x;
    // SPIR-V GLSL Fma need not supply the fused rounding residual we need.
    // Split into 12-bit significands instead, keeping every partial product.
    precise float ah = uintBitsToFloat(floatBitsToUint(a.x) & 0xfffff000u);
    precise float bh = uintBitsToFloat(floatBitsToUint(b.x) & 0xfffff000u);
    precise float al = a.x - ah;
    precise float bl = b.x - bh;
    precise float error1 = high - ah * bh;
    precise float error2 = error1 - al * bh;
    precise float error3 = error2 - ah * bl;
    precise float remainder = al * bl - error3;
    precise float cross = a.x * b.y + a.y * b.x;
    precise float low = remainder + cross;
    return vec2(high, low);
}

bool inside_edge(vec2 a, vec2 b, vec2 position) {
    precise vec2 dx = difference(b.x, a.x);
    precise vec2 dy = difference(b.y, a.y);
    precise vec2 px = difference(position.x, a.x);
    precise vec2 py = difference(position.y, a.y);
    precise vec2 left = product(dx, py);
    precise vec2 right = product(dy, px);
    precise vec2 delta = difference(left.x, right.x);
    precise float remainder = (left.y - right.y) + delta.y;
    precise float value = delta.x + remainder;
    return value > 0.0 || (value == 0.0 && (b.y < a.y || (b.y == a.y && b.x > a.x)));
}

void main() {
    if (triangle_kind != 0u &&
        !(inside_edge(triangle_a, triangle_b, gl_FragCoord.xy) &&
          inside_edge(triangle_b, triangle_c, gl_FragCoord.xy) &&
          inside_edge(triangle_c, triangle_a, gl_FragCoord.xy))) discard;
    uvec4 src = uvec4(floor(fragment_color * 255.0 + 0.5));
    if (src.a == 255u) { output_color = fragment_color; return; }
    ivec2 pixel = clamp(ivec2(gl_FragCoord.xy), ivec2(0), textureSize(previous_color, 0) - 1);
    uvec4 dst = uvec4(floor(texelFetch(previous_color, pixel, 0) * 255.0 + 0.5));
    if (src.a == 0u) { output_color = vec4(dst) / 255.0; return; }
    if (dst.a == 0u) { output_color = fragment_color; return; }
    uint inverse = 255u - src.a;
    uint alpha_numerator = src.a * 255u + dst.a * inverse;
    uvec3 numerator = src.rgb * src.a * 255u + dst.rgb * dst.a * inverse;
    uvec3 rgb = (numerator + alpha_numerator / 2u) / alpha_numerator;
    output_color = vec4(uvec4(rgb, (alpha_numerator + 127u) / 255u)) / 255.0;
}
