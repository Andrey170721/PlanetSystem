
// shader.hlsl

cbuffer CB : register(b0)
{
    matrix world;
    matrix view;
    matrix proj;
};

// цвета для градиента
cbuffer GradCB : register(b1)
{
    float4 gradBottom; // цвет на уровне y = 0
    float4 gradTop; // цвет на уровне y = H
    float gradHeight; // H
};

Texture2D gDiffuse : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float3 Pos : POSITION;
    float4 Color : COLOR; // не будет использоваться для градиента
    float2 UV : TEXCOORD0; // для моделей
};

struct PSInput
{
    float4 PosH : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 NormalWS : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

// единый VS для всего
PSInput VSMain(VSInput IN)
{
    PSInput OUT;
    float4 worldPos = mul(float4(IN.Pos, 1), world);
    OUT.PosH = mul(mul(worldPos, view), proj);
    OUT.WorldPos = worldPos.xyz;
    OUT.NormalWS = mul(IN.Pos, (float3x3) world);
    OUT.UV = IN.UV;
    return OUT;
}

// ———————————————————————————————————————————————————————————
// 1) Пиксельный шейдер для градиента (пол + катамари)
// ———————————————————————————————————————————————————————————
float4 PSGradient(PSInput IN) : SV_TARGET
{
    // нормализуем высоту (0…gradHeight) → 0…1
    float t = saturate(IN.WorldPos.y / gradHeight);
    return lerp(gradBottom, gradTop, t);
}

// ———————————————————————————————————————————————————————————
// 2) Пиксельный шейдер для текстурированных моделей
// ———————————————————————————————————————————————————————————
float4 PSTextured(PSInput IN) : SV_TARGET
{
    return gDiffuse.Sample(gSampler, IN.UV);
}