// Simple vertex+pixel shader
struct VS_IN
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
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

struct Light
{
    float3 direction;
    float pad1;
    float3 color;
    float pad2;
};

struct Material
{
    float3 ambient;
    float pad1;
    float3 diffuse;
    float pad2;
    float3 specular;
    float specPower;
};

cbuffer LightBuffer : register(b1)
{
    Light dirLight;
    Material mat;
    float3 viewPos;
    float pad3;
};

struct VS_OUT
{
    
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD1;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

VS_OUT VSMain(VS_IN IN)
{
    VS_OUT OUT;
    float4 worldPos = mul(float4(IN.Pos, 1), world);
    OUT.pos = mul(mul(worldPos, view), proj);
    OUT.worldPos = worldPos.xyz;
    OUT.normal = mul(IN.Normal, (float3x3) world); // world-space normal
    OUT.uv = IN.TexUV;
    return OUT;
}

float4 PSMain(VS_OUT IN) : SV_Target
{
    float3 N = normalize(IN.normal);
    float3 L = normalize(-dirLight.direction);
    float NdotL = max(dot(N, L), 0);
    
    // Ambient
    float3 ambient = dirLight.color * mat.ambient;
    // Diffuse
    float3 diffuse = dirLight.color * mat.diffuse * NdotL;
    // Specular
    float3 V = normalize(viewPos - IN.worldPos);
    float3 R = reflect(-L, N);
    float spec = pow(max(dot(R, V), 0), mat.specPower);
    float3 specular = dirLight.color * mat.specular * spec;
    
    float4 tex = g_Texture.Sample(g_Sampler, IN.uv);
    float3 color = (ambient + diffuse + specular) * tex.rgb;
    return float4(color, tex.a);
}