sampler2D scene : register(s0);
float4 params : register(c0); // visibility, reticle half-width, capture coverage

float4 main(float2 aperture : TEXCOORD0, float2 uv : TEXCOORD1) : COLOR0 {
    float r = length(aperture);
    clip(1 - r);
    float3 color = tex2D(scene, uv).rgb;
    // Coverage cancels the capture FOV expansion for a constant reticle.
    float2 reticle = (uv - .5) * params.z;
    float2 d = abs(reticle);
    float center = 1 - step(params.y * 1.5, length(reticle));
    float crosshair = max(
        (1 - step(params.y, d.x)) * step(params.y * 4, d.y) * (1 - step(.23, d.y)),
        (1 - step(params.y, d.y)) * step(params.y * 4, d.x) * (1 - step(.23, d.x)));
    color = lerp(color, float3(1, .84, .37), saturate(center + crosshair));
    // The capture now covers the aperture for both eyes. Only the round
    // physical lens defines its edge; do not stamp a second rectangular mask.
    color *= params.x * saturate((1 - r) * 35);
    return float4(color, 1);
}
