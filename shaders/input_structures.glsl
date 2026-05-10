layout(set = 0, binding = 0) uniform  SceneData{

	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 lightViewProj;      // sun's view-projection for shadow mapping
	vec4 ambientColor;       // sky color (hemispheric ambient: surfaces facing up)
	vec4 sunlightDirection;  // w for sun power
	vec4 sunlightColor;
	vec4 cameraPos;
	vec4 groundColor;
} sceneData;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;

layout(set = 1, binding = 0) uniform GLTFMaterialData{

	vec4 colorFactors;
	vec4 metal_rough_factors;

} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;

// Reverse-Z aware shadow lookup. Returns 1.0 = lit, 0.0 = shadowed.
// `lsPos` is the fragment's position in light clip space (sceneData.lightViewProj * worldPos).
float sampleShadow(vec4 lsPos)
{
	vec3 p = lsPos.xyz / lsPos.w;
	// Light-space NDC -> [0,1] for X/Y. Z stays in [0,1] for Vulkan.
	p.xy = p.xy * 0.5 + 0.5;
	if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
		return 1.0;
	if (p.z < 0.0 || p.z > 1.0)
		return 1.0;
	float occluder = texture(shadowMap, p.xy).r;
	// Reverse-Z: closer to light = larger depth value. Fragment is occluded
	// when it is farther from the light than the recorded occluder.
	float bias = 0.0005;
	return (p.z + bias < occluder) ? 0.0 : 1.0;
}
