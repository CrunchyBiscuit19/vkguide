#version 450

#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;

void main() 
{

	Vertex v = constants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	mat4 transformationMatrix = constants.transformBuffer.transforms[constants.meshIndex];
	
	mat4 translation = constants.instanceBuffer.instances[gl_InstanceIndex].translation;
	mat4 rotation = constants.instanceBuffer.instances[gl_InstanceIndex].rotation;
	mat4 scale = constants.instanceBuffer.instances[gl_InstanceIndex].scale;
	mat4 instanceMatrix = translation * rotation * scale;
	
	mat4 proj = constants.sceneBuffer.sceneData.proj; 
	mat4 view = constants.sceneBuffer.sceneData.view; 

	gl_Position =  proj * view * (instanceMatrix * transformationMatrix) * position; 

	outNormal = normalize(v.normal);
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
}