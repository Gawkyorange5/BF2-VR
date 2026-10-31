sampler2D hud : register(s0);
float4 strength : register(c0);
float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float4 c = tex2D(hud, uv);
    // BF2 captured RGB was multiplied by alpha in display/gamma space.
    // Restore straight display color BEFORE OpenXR performs sRGB decoding.
    // Submit this texture with UNPREMULTIPLIED_ALPHA so the runtime applies
    // alpha in linear space; do not apply gamma to already darkened RGB.
    float3 straight = c.rgb / max(c.a, 1.0 / 255.0);
    return float4(straight, saturate(c.a * strength.x));
}
