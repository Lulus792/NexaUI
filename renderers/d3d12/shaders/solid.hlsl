struct V { float2 pos : POSITION; float4 color : COLOR; };
struct P { float4 pos : SV_POSITION; float4 color : COLOR; };
P VSMain(V v) { P p; p.pos = float4(v.pos, 0, 1); p.color = v.color; return p; }
float4 PSMain(P p) : SV_TARGET { return p.color; }
