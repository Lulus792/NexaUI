Texture2D<float4> previous : register(t0);
struct V { float2 pos : POSITION; float4 color : COLOR; };
struct P { float4 pos : SV_POSITION; float4 color : COLOR; };
P VSMain(V v) { P p; p.pos = float4(v.pos, 0, 1); p.color = v.color; return p; }
float4 PSMain(P p) : SV_TARGET {
  uint4 s = (uint4)round(saturate(p.color) * 255.0);
  uint4 d = (uint4)round(saturate(previous.Load(int3(int2(p.pos.xy), 0))) * 255.0);
  if (s.a == 0) return float4(d) / 255.0;
  if (s.a == 255 || d.a == 0) return float4(s) / 255.0;
  uint inv = 255 - s.a;
  uint denom = s.a * 255 + d.a * inv;
  uint3 rgb = (s.rgb * s.a * 255 + d.rgb * d.a * inv + denom / 2) / denom;
  uint alpha = (denom + 127) / 255;
  return float4(rgb, alpha) / 255.0;
}
