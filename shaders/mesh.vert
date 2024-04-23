#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;

layout (location = 3) out vec4 outAmbientColor;
layout (location = 4) out vec4 outSunlightDirection;
layout (location = 5) out vec4 outSunlightColor;

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
struct SceneData {   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; // w for sun power
	vec4 sunlightColor;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};
layout(buffer_reference, std430) readonly buffer InstanceBuffer{ 
	Instance instances[];
};
layout(buffer_reference, std430) readonly buffer SceneBuffer{ 
	SceneData sceneData;
};

// Push constants block
layout( push_constant, std430 ) uniform PushConstants
{
	VertexBuffer vertexBuffer;
	InstanceBuffer instanceBuffer;
	SceneBuffer sceneBuffer;
} constants;

void main() 
{
	Vertex v = constants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	mat4 renderMatrix = constants.instanceBuffer.instances[gl_InstanceIndex].translation * constants.instanceBuffer.instances[gl_InstanceIndex].rotation * constants.instanceBuffer.instances[gl_InstanceIndex].scale;
	gl_Position =  constants.sceneBuffer.sceneData.viewproj * renderMatrix * position; // pvm matrices

	outNormal = mat3(transpose(inverse(renderMatrix))) * v.normal;
	outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;

	outAmbientColor = constants.sceneBuffer.sceneData.ambientColor;
	outSunlightColor = constants.sceneBuffer.sceneData.sunlightColor;
	outSunlightDirection = constants.sceneBuffer.sceneData.sunlightDirection;
}