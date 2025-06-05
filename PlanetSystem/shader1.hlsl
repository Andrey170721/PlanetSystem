#define MAX_POINT_LIGHTS 32

struct PointLight
{
    float3 position;
    float range;
    float3 color;
    float intensity;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
};

cbuffer LightBuffer : register(b1)
{
    float3 dirLightDir;
    float pad0;
    float3 dirLightColor;
    float pad1;

    float3 matAmbient;
    float pad2;
    float3 matDiffuse;
    float pad3;
    float3 matSpecular;
    float matSpecPower;

    float3 viewPos;
    float pad4;

    int pointCount;
    float3 padPoint;

    PointLight pLights[MAX_POINT_LIGHTS];
};

cbuffer ShadowBuffer : register(b2)
{
    float4x4 lightViewProj;
    float shadowBias;
    float3 _pad;
};

Texture2D diffuseMap : register(t0);
Texture2D shadowTex : register(t1);
SamplerState sampLinear : register(s0);
SamplerComparisonState shadowSamp : register(s1);

struct VS_IN
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 svPos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float4 lightPos : TEXCOORD2;
};

VS_OUT VSMain(VS_IN IN)
{
    VS_OUT OUT;
    float4 wPos = mul(float4(IN.pos, 1.0f), world);
    OUT.worldPos = wPos.xyz;
    OUT.svPos = mul(wPos, view);
    OUT.svPos = mul(OUT.svPos, projection);

    OUT.normal = mul(IN.normal, (float3x3) world);
    OUT.uv = IN.uv;
    OUT.lightPos = mul(wPos, lightViewProj);

    return OUT;
}

float4 PSMain(VS_OUT IN) : SV_TARGET
{
    float3 N = normalize(IN.normal);
    float3 V = normalize(viewPos - IN.worldPos);
    float3 albedo = diffuseMap.Sample(sampLinear, IN.uv).rgb;
    float3 color = albedo * matAmbient;

    float3 Ld = normalize(-dirLightDir);
    float NdotL = saturate(dot(N, Ld));
    float3 diff = albedo * dirLightColor * NdotL;

    float3 cp = IN.lightPos.xyz / IN.lightPos.w;
    float2 shadowUV = cp.xy * 0.5f + 0.5f;
    float shadowDepth = cp.z;
    float visibility = shadowTex.SampleCmpLevelZero(shadowSamp, shadowUV, shadowDepth - shadowBias);
    diff *= visibility;
    color += diff;

    [loop]
    for (int i = 0; i < pointCount; ++i)
    {
        float3 Lvec = pLights[i].position - IN.worldPos;
        float dist = length(Lvec);
        float3 L = Lvec / dist;

        float atten = saturate(1.0f - dist / pLights[i].range);
        float lambert = saturate(dot(N, L));

        color += albedo * pLights[i].color * pLights[i].intensity * lambert * atten;
    }

    return float4(color, 1.0f);
}

float4 VSDepthOnly(float3 pos : POSITION) : SV_POSITION
{
    return mul(mul(float4(pos, 1), world), lightViewProj);
}