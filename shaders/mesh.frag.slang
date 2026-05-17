import common;

// Mirrors GPUMeshShaderPushConstants (vk_types.h). Only `debugFlags` is read
// in the fragment stage; the buffer-address fields are layout-padding here.
struct Meshlet
{
    uint vertexOffset;
    uint triangleOffset;
    uint vertexCount;
    uint triangleCount;
};

struct MeshletBounds
{
    float4 centerRadius;
    float4 coneApex;
    float4 coneAxisCutoff;
};

struct PushConstants
{
    float4x4           worldMatrix;
    Vertex*            vertexBuffer;
    InstanceTransform* instanceTransformBuffer;
    Meshlet*           meshlets;
    uint*              meshletVertices;
    uint*              meshletTriangles;
    MeshletBounds*     meshletBounds;
    uint               meshletCount;
    uint               instanceCount;
    uint               debugFlags;       // bit0 = cluster color, bit1 = lit cluster color
};
[[vk::push_constant]] PushConstants pc;

static const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz normal distribution
float D_GGX(float NoH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Schlick-GGX geometry term (UE4 remap for direct lighting)
float G_SchlickGGX(float NoX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NoX / (NoX * (1.0 - k) + k);
}

float G_Smith(float NoV, float NoL, float roughness)
{
    return G_SchlickGGX(NoV, roughness) * G_SchlickGGX(NoL, roughness);
}

float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

struct FSIn
{
    float3 normal      : NORMAL;
    float3 color       : COLOR;
    float2 uv          : TEXCOORD0;
    float3 worldPos    : TEXCOORD1;
    float4 shadowCoord : TEXCOORD2;
};

[shader("fragment")]
float4 main(FSIn i) : SV_Target
{
    // Debug view: flat cluster color (cluster-color on, lit-cluster-color off).
    // Bypasses lighting / tonemap / gamma so meshlet boundaries stay crisp.
    if ((pc.debugFlags & 0x1u) != 0u && (pc.debugFlags & 0x2u) == 0u)
    {
        return float4(i.color, 1.0);
    }

    float3 albedo    = i.color * colorTex.Sample(i.uv).rgb;
    float3 mr        = metalRoughTex.Sample(i.uv).rgb;
    float  metallic  = mr.b * materialData.metalRoughFactors.x;
    float  roughness = clamp(mr.g * materialData.metalRoughFactors.y, 0.04, 1.0);

    float3 N = normalize(i.normal);
    float3 V = normalize(sceneData.cameraPos.xyz - i.worldPos);
    float3 L = normalize(sceneData.sunlightDirection.xyz);
    float3 H = normalize(V + L);

    float NoV = max(dot(N, V), 0.0);
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    // Dielectrics get F0 = 0.04, metals use albedo as F0.
    float3 F0 = lerp(float3(0.04), albedo, metallic);

    float  D = D_GGX(NoH, roughness);
    float  G = G_Smith(NoV, NoL, roughness);
    float3 F = F_Schlick(VoH, F0);

    float3 specular = (D * G) * F / max(4.0 * NoV * NoL, 1e-4);

    // Energy split: metals have no diffuse.
    float3 kd      = (float3(1.0) - F) * (1.0 - metallic);
    float3 diffuse = kd * albedo / PI;

    float3 sun    = sceneData.sunlightColor.rgb * sceneData.sunlightColor.w;
    float  shadow = sampleShadow(i.shadowCoord);
    float3 Lo     = (diffuse + specular) * sun * NoL * shadow;

    // Hemispheric ambient: blend sky/ground by N.y.
    float  upMix   = N.y * 0.5 + 0.5;
    float3 hemi    = lerp(sceneData.groundColor.rgb, sceneData.ambientColor.rgb, upMix);
    float3 ambient = hemi * albedo * (1.0 - metallic);

    // Cheap fill light from opposite side of the sun (horizontal mirror).
    float3 fillDir = normalize(float3(-L.x, max(L.y * 0.3, 0.0), -L.z));
    float  fillNoL = max(dot(N, fillDir), 0.0);
    float3 fill    = albedo * fillNoL * 0.35 * (1.0 - metallic);

    float3 color = ambient + fill + Lo;

    // Reinhard tonemap then gamma encode.
    color = color / (color + float3(1.0));
    color = pow(color, float3(1.0 / 2.2));

    return float4(color, 1.0);
}
