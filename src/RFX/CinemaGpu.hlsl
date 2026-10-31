// Match CleanVR's previous CPU HasPicture sample grid and >8/255 RGB test.
// Precompiled offline; no shader compiler DLL is added to BF2's runtime.
cbuffer Samples : register(b0) { uint2 step; uint2 padding; };
Texture2D<float4> picture : register(t0);
float4 vs(uint id:SV_VertexID):SV_Position {
    float2 p=float2((id<<1)&2,id&2);
    return float4(p*float2(2,-2)+float2(-1,1),0,1);
}
float4 ps(float4 p:SV_Position):SV_Target {
    float3 rgb=picture.Load(int3(uint2(p.xy)*step,0)).rgb;
    if(max(rgb.r,max(rgb.g,rgb.b))<=8.0/255.0)discard;
    return 0;
}
