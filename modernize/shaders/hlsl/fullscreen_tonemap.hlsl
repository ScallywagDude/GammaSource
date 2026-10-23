// Source modernization rendering experiment (not installed as an engine pass).
// D3D11: compile VSMain vs_5_0 and PSMain ps_5_0 using fxc.
// D3D12: compile VSMain vs_6_0 and PSMain ps_6_0 using dxc.
// Bind t0/s0 and b0. Render to an sRGB target when possible, then set
// controls.z=0; for a linear UNORM target use controls.z=1.
Texture2D SceneColor : register(t0);
SamplerState SceneSampler : register(s0);
cbuffer TonemapConstants : register(b0)
{
    // x: exposure, y: apply filmic curve, z: manual sRGB encode, w: reserved
    float4 controls;
};
struct VSOutput { float4 position : SV_Position; float2 uv : TEXCOORD0; };
VSOutput VSMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.uv = uv;
    output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    return output;
}
float3 ACESFilmic(float3 x)
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}
float3 EncodeSRGB(float3 x)
{
    x = saturate(x);
    return lerp(12.92 * x, 1.055 * pow(x, 1.0 / 2.4) - 0.055, step(0.0031308, x));
}
float4 PSMain(VSOutput input) : SV_Target0
{
    float4 scene = SceneColor.Sample(SceneSampler, input.uv);
    float3 linearColor = max(scene.rgb, float3(0, 0, 0)) * max(controls.x, 0.0);
    float3 mapped = controls.y > 0.5 ? ACESFilmic(linearColor) : saturate(linearColor);
    return float4(controls.z > 0.5 ? EncodeSRGB(mapped) : mapped, scene.a);
}
