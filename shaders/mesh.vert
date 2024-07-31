#version 450

#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outColor;

void main() 
{

	Vertex v = constants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	mat4 transformationMatrix = constants.transformBuffer.transforms[constants.meshIndex];
	
	mat4 instanceMatrix = constants.instanceBuffer.instances[gl_InstanceIndex].transformation;
	
	mat4 proj = constants.sceneBuffer.sceneData.proj; 
	mat4 view = constants.sceneBuffer.sceneData.view; 

	uint materialFactorIndex = constants.materialIndex;
	vec4 baseFactor = constants.materialBuffer.materials[materialFactorIndex].baseFactor;

	gl_Position =  proj * view * (instanceMatrix * transformationMatrix) * position; 

	outColor = v.color * baseFactor;
	outNormal = normalize(v.normal);
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
}