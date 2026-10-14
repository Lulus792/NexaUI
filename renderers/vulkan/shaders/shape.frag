#version 450

layout(location = 0) flat in vec4 fragment_color;
layout(location = 1) flat in vec2 shape_a;
layout(location = 2) flat in vec2 shape_b;
layout(location = 3) flat in vec2 shape_c;
layout(location = 4) flat in uint shape_kind;
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

// Two-component arithmetic keeps near-boundary shape tests independent of
// optional shaderFloat64. NoContraction also applies to these operations.
vec2 add_pair(vec2 a, vec2 b) {
    precise float high = a.x + b.x;
    precise float bv = high - a.x;
    precise float low = (a.x - (high - bv)) + (b.x - bv);
    precise float tail = (low + a.y) + b.y;
    precise float value = high + tail;
    precise float rest = tail - (value - high);
    return vec2(value, rest);
}

vec2 multiply_pair(vec2 a, vec2 b) {
    precise vec2 base = product(a, b);
    precise float tail = a.y * b.y;
    return add_pair(base, vec2(tail, 0.0));
}

bool negative_pair(vec2 a) { return a.x < 0.0 || (a.x == 0.0 && a.y < 0.0); }
bool less_pair(vec2 a, vec2 b) { return negative_pair(add_pair(a, -b)); }
bool disk_contains(vec2 dx, vec2 dy, float radius) {
    vec2 distance2 = add_pair(multiply_pair(dx, dx), multiply_pair(dy, dy));
    return !less_pair(multiply_pair(vec2(radius, 0.0), vec2(radius, 0.0)), distance2);
}

bool shape_contains(vec2 p) {
    if (shape_kind == 1u)
        return inside_edge(shape_a, shape_b, p) &&
               inside_edge(shape_b, shape_c, p) &&
               inside_edge(shape_c, shape_a, p);
    vec2 x = difference(p.x, shape_a.x);
    vec2 y = difference(p.y, shape_a.y);
    float radius = shape_c.x;
    if (shape_kind == 2u) {
        vec2 inset = vec2(radius, 0.0);
        return less_pair(x, inset) || less_pair(y, inset) ||
               !less_pair(x, difference(shape_b.x, radius)) ||
               !less_pair(y, difference(shape_b.y, radius));
    }
    if (shape_kind == 3u) {
        vec2 r = vec2(radius, 0.0);
        vec2 right = difference(shape_b.x, radius);
        vec2 bottom = difference(shape_b.y, radius);
        vec2 dx = less_pair(x, r) ? add_pair(x, -r) : less_pair(right, x) ? add_pair(x, -right) : vec2(0.0);
        vec2 dy = less_pair(y, r) ? add_pair(y, -r) : less_pair(bottom, y) ? add_pair(y, -bottom) : vec2(0.0);
        return disk_contains(dx, dy, radius);
    }
    if (shape_kind == 4u) {
        vec2 dx = difference(shape_b.x, shape_a.x);
        vec2 dy = difference(shape_b.y, shape_a.y);
        vec2 length2 = add_pair(multiply_pair(dx, dx), multiply_pair(dy, dy));
        vec2 along = add_pair(multiply_pair(x, dx), multiply_pair(y, dy));
        if (!less_pair(vec2(0.0), along)) return disk_contains(x, y, radius);
        if (!less_pair(along, length2))
            return disk_contains(difference(p.x, shape_b.x), difference(p.y, shape_b.y), radius);
        vec2 cross = add_pair(multiply_pair(dx, y), -multiply_pair(dy, x));
        vec2 r2 = multiply_pair(vec2(radius, 0.0), vec2(radius, 0.0));
        return !less_pair(multiply_pair(r2, length2), multiply_pair(cross, cross));
    }
    return true;
}

void main() {
    if (!shape_contains(gl_FragCoord.xy)) discard;
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
