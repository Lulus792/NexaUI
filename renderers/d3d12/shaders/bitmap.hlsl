Texture2D<float4> previous : register(t0);
Texture2D<float4> source : register(t1);
cbuffer Bitmap : register(b0) { float4 bounds; uint source_format; };
float identity_position(float position, float origin) {
  precise float whole = floor(origin);
  precise float midpoint = whole + 0.5;
  float bias = -whole;
  if (origin > midpoint) bias -= 1.0;
  return floor(position) + bias;
}
struct V { float2 pos : POSITION; float4 color : COLOR; };
struct P { float4 pos : SV_POSITION; float4 color : COLOR; };
P VSMain(V v) { P p; p.pos = float4(v.pos, 0, 1); p.color = v.color; return p; }
float4 PSMain(P p) : SV_TARGET {
  uint width, height; source.GetDimensions(width, height);
  float2 local = p.pos.xy - bounds.xy;
  precise float2 sample_position = local / bounds.zw * float2(width, height);
  if (bounds.z == float(width)) sample_position.x = identity_position(p.pos.x, bounds.x);
  if (bounds.w == float(height)) sample_position.y = identity_position(p.pos.y, bounds.y);
  uint2 xy = min((uint2)max(floor(sample_position), 0.0), uint2(width - 1, height - 1));
  uint4 sample = (uint4)round(saturate(source.Load(int3(xy, 0))) * 255.0);
  uint4 tint = (uint4)round(saturate(p.color) * 255.0);
  uint4 s;
  if (source_format == 2) { s.rgb = tint.rgb; s.a = (sample.r * tint.a + 127) / 255; }
  else { s = (sample * tint + 127) / 255; }
  uint4 d = (uint4)round(saturate(previous.Load(int3(int2(p.pos.xy), 0))) * 255.0);
  if (s.a == 0) return float4(d) / 255.0;
  if (s.a == 255 || d.a == 0) return float4(s) / 255.0;
  uint inv = 255 - s.a;
  uint denom = s.a * 255 + d.a * inv;
  uint3 rgb = (s.rgb * s.a * 255 + d.rgb * d.a * inv + denom / 2) / denom;
  uint alpha = (denom + 127) / 255;
  return float4(rgb, alpha) / 255.0;
}
