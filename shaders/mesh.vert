#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;

// Buffer device addresses
struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 
struct Instance {
	mat4 translation;
    mat4 rotation;
    mat4 scale;
};
layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};
layout(buffer_reference, std430) readonly buffer InstanceBuffer{ 
	Instance instances[];
};

// Push constants block
layout( push_constant, std430 ) uniform constants
{
	VertexBuffer vertexBuffer;
	InstanceBuffer instanceBuffer;
} PushConstants;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	mat4 renderMatrix = PushConstants.instanceBuffer.instances[gl_InstanceIndex].translation * PushConstants.instanceBuffer.instances[gl_InstanceIndex].rotation * PushConstants.instanceBuffer.instances[gl_InstanceIndex].scale;
	gl_Position =  sceneData.viewproj * renderMatrix * position; // pvm matrices

	outNormal = mat3(transpose(inverse(renderMatrix))) * v.normal;
	outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
}