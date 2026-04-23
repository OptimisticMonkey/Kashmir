#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inWorldPos;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz normal distribution
float D_GGX(float NoH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Schlick-GGX geometry term (UE4 remap for direct lighting)
float G_SchlickGGX(float NoX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NoX / (NoX * (1.0 - k) + k);
}

float G_Smith(float NoV, float NoL, float roughness) {
    return G_SchlickGGX(NoV, roughness) * G_SchlickGGX(NoL, roughness);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec3 albedo    = inColor * texture(colorTex, inUV).rgb;
    vec3 mr        = texture(metalRoughTex, inUV).rgb;
    float metallic  = mr.b * materialData.metal_rough_factors.x;
    float roughness = clamp(mr.g * materialData.metal_rough_factors.y, 0.04, 1.0);

    vec3 N = normalize(inNormal);
    vec3 V = normalize(sceneData.cameraPos.xyz - inWorldPos);
    vec3 L = normalize(sceneData.sunlightDirection.xyz);
    vec3 H = normalize(V + L);

    float NoV = max(dot(N, V), 0.0);
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    // Dielectrics get F0 = 0.04, metals use albedo as F0.
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = D_GGX(NoH, roughness);
    float G = G_Smith(NoV, NoL, roughness);
    vec3  F = F_Schlick(VoH, F0);

    vec3  specular = (D * G) * F / max(4.0 * NoV * NoL, 1e-4);

    // Energy split: metals have no diffuse.
    vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kd * albedo / PI;

    vec3 sun = sceneData.sunlightColor.rgb * sceneData.sunlightColor.w;
    vec3 Lo  = (diffuse + specular) * sun * NoL;

    // Hemispheric ambient: blend sky/ground by N.y.
    float upMix = N.y * 0.5 + 0.5;
    vec3 hemi = mix(sceneData.groundColor.rgb, sceneData.ambientColor.rgb, upMix);
    vec3 ambient = hemi * albedo * (1.0 - metallic);

    // Cheap fill light from opposite side of the sun (horizontal mirror).
    vec3 fillDir = normalize(vec3(-L.x, max(L.y * 0.3, 0.0), -L.z));
    float fillNoL = max(dot(N, fillDir), 0.0);
    vec3 fill = albedo * fillNoL * 0.35 * (1.0 - metallic);

    vec3 color = ambient + fill + Lo;

    // Reinhard tonemap then gamma encode (helps banding by spending bits in perceptual space).
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outFragColor = vec4(color, 1.0);
}
