#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec4 ambientColor = constants.sceneBuffer.sceneData.ambientColor;
	vec4 sunlightColor = constants.sceneBuffer.sceneData.sunlightColor;
	vec4 sunlightDirection = constants.sceneBuffer.sceneData.sunlightDirection;
	int materialIndex = constants.materialIndex;
	vec4 baseFactor = constants.materialBuffer.material.baseFactor;

	float lightValue = max(dot(inNormal, sunlightDirection.xyz), 0.1f);
	
	vec4 color = texture(materialTextures[materialIndex], inUV) * baseFactor;
	vec4 ambient = color * ambientColor;

	outFragColor = vec4(color * lightValue * sunlightColor.w + ambient);
}
