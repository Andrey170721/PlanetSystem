// Simple vertex+pixel shader
struct VS_IN
{
    float3 Pos : POSITION;
    float2 TexUV : TEXCOORD0;
};

struct PS_IN
{
    float4 Pos : SV_POSITION;
    float2 TexUV : TEXCOORD0;
};

Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix world;
    matrix view;
    matrix proj;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;
    float4 posW = mul(float4(input.Pos, 1.0f), world);
    output.Pos = mul(mul(posW, view), proj);
    output.TexUV = input.TexUV;
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    return g_Texture.Sample(g_Sampler, input.TexUV);
}