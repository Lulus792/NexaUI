Texture2D<float4> previous : register(t0);
cbuffer Shape : register(b0) { float4 bounds; float radius; uint kind; };
struct V { float2 pos : POSITION; float4 color : COLOR; };
struct P { float4 pos : SV_POSITION; float4 color : COLOR; };
P VSMain(V v) { P p; p.pos = float4(v.pos, 0, 1); p.color = v.color; return p; }
float4 PSMain(P p) : SV_TARGET {
  float2 diff;
  if (kind == 0) {
    float2 a = bounds.xy, d = bounds.zw - a;
    float len2 = dot(d, d);
    float along = len2 > 0 ? saturate(dot(p.pos.xy - a, d) / len2) : 0;
    diff = p.pos.xy - (a + along * d);
  } else {
    float2 nearest = clamp(p.pos.xy, bounds.xy + radius, bounds.zw - radius);
    diff = p.pos.xy - nearest;
  }
  if (dot(diff, diff) > radius * radius) discard;
  uint4 s = (uint4)round(saturate(p.color) * 255.0);
  uint4 old = (uint4)round(saturate(previous.Load(int3(int2(p.pos.xy), 0))) * 255.0);
  if (s.a == 0) return float4(old) / 255.0;
  if (s.a == 255 || old.a == 0) return float4(s) / 255.0;
  uint inv = 255 - s.a;
  uint denom = s.a * 255 + old.a * inv;
  uint3 rgb = (s.rgb * s.a * 255 + old.rgb * old.a * inv + denom / 2) / denom;
  uint alpha = (denom + 127) / 255;
  return float4(rgb, alpha) / 255.0;
}
