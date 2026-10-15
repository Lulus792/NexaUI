// NexaUI geometry/composition. Matches draw_plan.h; no shader translation library.
#include <metal_stdlib>
using namespace metal;
#pragma STDC FP_CONTRACT OFF

struct Shape {
    packed_float2 a, b, c;
    uint kind;
    uint color[4];
    packed_float2 bitmap_size;
};
static_assert(sizeof(Shape) == 52, "C/MSL shape layout");
struct Raster { float4 position [[position]]; };
vertex Raster nx_mtl_vertex(uint index [[vertex_id]], constant packed_float2 *vertices [[buffer(0)]]) {
    Raster out;
    out.position = float4(float2(vertices[index]), 0.0, 1.0);
    return out;
}

// Keep the rounding remainder when two large edge products nearly cancel.
// This uses float32 operations, without requiring double-precision shaders.
float2 difference(float a, float b) {
    float high = a - b;
    float bv = a - high;
    float av = high + bv;
    float br = bv - b;
    float ar = a - av;
    float low = ar + br;
    return float2(high, low);
}

float2 product(float2 a, float2 b) {
    float high = a.x * b.x;
    // Use explicitly split float32 products for the rounding residual.
    // Split into 12-bit significands instead, keeping every partial product.
    float ah = as_type<float>(as_type<uint>(a.x) & 0xfffff000u);
    float bh = as_type<float>(as_type<uint>(b.x) & 0xfffff000u);
    float al = a.x - ah;
    float bl = b.x - bh;
    float error1 = high - ah * bh;
    float error2 = error1 - al * bh;
    float error3 = error2 - ah * bl;
    float remainder = al * bl - error3;
    float cross = a.x * b.y + a.y * b.x;
    float low = remainder + cross;
    return float2(high, low);
}

bool inside_edge(float2 a, float2 b, float2 position) {
    float2 dx = difference(b.x, a.x);
    float2 dy = difference(b.y, a.y);
    float2 px = difference(position.x, a.x);
    float2 py = difference(position.y, a.y);
    float2 left = product(dx, py);
    float2 right = product(dy, px);
    float2 delta = difference(left.x, right.x);
    float remainder = (left.y - right.y) + delta.y;
    float value = delta.x + remainder;
    return value > 0.0 || (value == 0.0 && (b.y < a.y || (b.y == a.y && b.x > a.x)));
}

// Two-component arithmetic keeps near-boundary shape tests independent of
// optional shaderFloat64. FP_CONTRACT OFF also applies to these operations.
float2 add_pair(float2 a, float2 b) {
    float high = a.x + b.x;
    float bv = high - a.x;
    float low = (a.x - (high - bv)) + (b.x - bv);
    float tail = (low + a.y) + b.y;
    float value = high + tail;
    float rest = tail - (value - high);
    return float2(value, rest);
}

float2 multiply_pair(float2 a, float2 b) {
    float2 base = product(a, b);
    float tail = a.y * b.y;
    return add_pair(base, float2(tail, 0.0));
}

bool negative_pair(float2 a) { return a.x < 0.0 || (a.x == 0.0 && a.y < 0.0); }
bool less_pair(float2 a, float2 b) { return negative_pair(add_pair(a, -b)); }
bool disk_contains(float2 dx, float2 dy, float radius) {
    float2 distance2 = add_pair(multiply_pair(dx, dx), multiply_pair(dy, dy));
    return !less_pair(multiply_pair(float2(radius, 0.0), float2(radius, 0.0)), distance2);
}

bool shape_contains(float2 p, constant Shape &s) {
    if (s.kind == 1u)
        return inside_edge(float2(s.a), float2(s.b), p) &&
               inside_edge(float2(s.b), float2(s.c), p) &&
               inside_edge(float2(s.c), float2(s.a), p);
    float2 x = difference(p.x, float2(s.a).x);
    float2 y = difference(p.y, float2(s.a).y);
    float radius = float2(s.c).x;
    if (s.kind == 2u) {
        float2 inset = float2(radius, 0.0);
        return less_pair(x, inset) || less_pair(y, inset) ||
               !less_pair(x, difference(float2(s.b).x, radius)) ||
               !less_pair(y, difference(float2(s.b).y, radius));
    }
    if (s.kind == 3u) {
        float2 r = float2(radius, 0.0);
        float2 right = difference(float2(s.b).x, radius);
        float2 bottom = difference(float2(s.b).y, radius);
        float2 dx = less_pair(x, r) ? add_pair(x, -r) : less_pair(right, x) ? add_pair(x, -right) : float2(0.0);
        float2 dy = less_pair(y, r) ? add_pair(y, -r) : less_pair(bottom, y) ? add_pair(y, -bottom) : float2(0.0);
        return disk_contains(dx, dy, radius);
    }
    if (s.kind == 4u) {
        float2 dx = difference(float2(s.b).x, float2(s.a).x);
        float2 dy = difference(float2(s.b).y, float2(s.a).y);
        float2 length2 = add_pair(multiply_pair(dx, dx), multiply_pair(dy, dy));
        float2 along = add_pair(multiply_pair(x, dx), multiply_pair(y, dy));
        if (!less_pair(float2(0.0), along)) return disk_contains(x, y, radius);
        if (!less_pair(along, length2))
            return disk_contains(difference(p.x, float2(s.b).x), difference(p.y, float2(s.b).y), radius);
        float2 cross = add_pair(multiply_pair(dx, y), -multiply_pair(dy, x));
        float2 r2 = multiply_pair(float2(radius, 0.0), float2(radius, 0.0));
        return !less_pair(multiply_pair(r2, length2), multiply_pair(cross, cross));
    }
    return true;
}

int sample_axis(float position, float origin, float extent, float texels) {
    float2 local = difference(position, origin);
    if (!less_pair(float2(0.0), local)) return 0;
    if (!less_pair(local, float2(extent, 0.0))) return int(texels) - 1;
    float2 numerator = multiply_pair(local, float2(texels, 0.0));
    float high = numerator.x / extent;
    float2 remainder = add_pair(numerator, -multiply_pair(float2(high, 0.0), float2(extent, 0.0)));
    float low = (remainder.x + remainder.y) / extent;
    float2 quotient = add_pair(float2(high, 0.0), float2(low, 0.0));
    float value = floor(quotient.x);
    if (value == quotient.x && quotient.y < 0.0) value -= 1.0;
    return int(clamp(value, 0.0, texels - 1.0));
}

fragment float4 nx_mtl_fragment(Raster in [[stage_in]], constant Shape &s [[buffer(0)]],
    texture2d<float, access::read> history [[texture(0)]], texture2d<float, access::read> atlas [[texture(1)]]) {
    if (!shape_contains(in.position.xy, s)) discard_fragment();
    uint4 src = uint4(s.color[0],s.color[1],s.color[2],s.color[3]);
    if (s.kind == 5u) {
        uint2 pixel = uint2(s.c) + uint2(sample_axis(in.position.x,s.a.x,s.b.x,s.bitmap_size.x),
            sample_axis(in.position.y,s.a.y,s.b.y,s.bitmap_size.y));
        uint4 sample_color = uint4(floor(atlas.read(pixel)*255.0+0.5));
        src = (sample_color*src+127u)/255u;
    }
    if (src.a == 255u) return float4(src) / 255.0;
    uint4 dst = uint4(floor(history.read(uint2(in.position.xy)) * 255.0 + 0.5));
    if (src.a == 0u) return float4(dst) / 255.0;
    if (dst.a == 0u) return float4(src) / 255.0;
    uint inverse = 255u - src.a;
    uint alpha_numerator = src.a * 255u + dst.a * inverse;
    uint3 numerator = src.rgb * src.a * 255u + dst.rgb * dst.a * inverse;
    uint3 rgb = (numerator + alpha_numerator / 2u) / alpha_numerator;
    return float4(uint4(rgb,(alpha_numerator + 127u) / 255u)) / 255.0;
}
