#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

struct InstanceTransform {
	mat4 transform;
};

layout(buffer_reference, std430) readonly buffer InstanceTransformBuffer{
	InstanceTransform transforms[];
};

// Same push constant layout as GPUDrawPushConstants — reuses the existing
// per-draw vertex/instance buffer addresses so shadow casters draw the same
// geometry as the camera pass.
layout( push_constant ) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
	InstanceTransformBuffer instanceTransformBuffer;
} PushConstants;

void main()
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	mat4 instance_transform = PushConstants.instanceTransformBuffer.transforms[gl_InstanceIndex].transform;
	vec4 worldPos = instance_transform * vec4(v.position, 1.0);
	gl_Position = sceneData.lightViewProj * worldPos;
}
