import common;

static const float PI = 3.14159265359;

float D_GGX(float NoH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

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
    // Ground.glb has zero vertex colors, so we ignore i.color here and rely on
    // the material's colorFactors * base color texture for albedo.
    float3 albedo = materialData.colorFactors.rgb * colorTex.Sample(i.uv).rgb;
    float3 mr     = metalRoughTex.Sample(i.uv).rgb;
    // Force ground to dielectric — the shared default material sets metallic=1
    // with a white metal/rough texture, which would zero out diffuse/ambient/fill.
    const float metallic  = 0.0;
    float       roughness = clamp(mr.g * materialData.metalRoughFactors.y, 0.04, 1.0);

    // Point light — Unreal-style inverse-square falloff with a smooth radius window.
    // Reduced intensity so the directional sun (and its shadows) dominates the
    // visible ground lighting; the point light now reads as a subtle accent.
    const float3 lightPos       = float3(0.0, 0.0, 0.0);
    const float  lightRadius    = 300.0;
    const float  lightIntensity = 2000.0;

    float3 toLight = lightPos - i.worldPos;
    float  distSq  = dot(toLight, toLight);
    float3 L       = toLight * rsqrt(max(distSq, 1e-8));

    // 1/(d^2+1) keeps the value finite at the source; windowed forces attenuation
    // to 0 at and beyond lightRadius (matches UE's InverseSquared falloff).
    float invSquare = 1.0 / (distSq + 1.0);
    float windowed  = clamp(1.0 - (distSq * distSq) /
                                  (lightRadius * lightRadius * lightRadius * lightRadius),
                            0.0, 1.0);
    windowed *= windowed;
    float attenuation = invSquare * windowed * lightIntensity;

    float3 N = normalize(i.normal);
    float3 V = normalize(sceneData.cameraPos.xyz - i.worldPos);
    float3 H = normalize(V + L);

    float NoV = max(dot(N, V), 0.0);
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    // Dielectrics get F0 = 0.04, metals use albedo as F0.
    float3 F0 = lerp(float3(0.04), albedo, metallic);

    float3 F = F_Schlick(VoH, F0);

    float3 specular = float3(0.0);

    // Energy split: metals have no diffuse.
    float3 kd      = (float3(1.0) - F) * (1.0 - metallic);
    float3 diffuse = kd * albedo / PI;

    float3 lightRadiance = sceneData.sunlightColor.rgb * attenuation;
    float3 Lo            = (diffuse + specular) * lightRadiance * NoL;

    Lo *= sampleShadow(i.shadowCoord);

    // Hemispheric ambient kept off (the GLSL source had it commented). Re-enable
    // by uncommenting if blacks crush too hard.
    // float  upMix   = N.y * 0.5 + 0.5;
    // float3 hemi    = lerp(sceneData.groundColor.rgb, sceneData.ambientColor.rgb, upMix);
    // float3 ambient = hemi * albedo * (1.0 - metallic);

    float3 color = Lo;

    // Reinhard tonemap then gamma encode.
    color = color / (color + float3(1.0));
    color = pow(color, float3(1.0 / 2.2));

    return float4(color, 1.0);
}
