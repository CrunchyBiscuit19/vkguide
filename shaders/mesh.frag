#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec4 ambientColor = constants.sceneBuffer.sceneData.ambientColor;
	vec4 sunlightColor = constants.sceneBuffer.sceneData.sunlightColor;
	vec4 sunlightDirection = constants.sceneBuffer.sceneData.sunlightDirection;

	uint materialFactorIndex = constants.materialIndex;
	uint materialTextureIndex = materialFactorIndex * 5;
	uint baseTextureIndex = materialTextureIndex + 0;

	float lightValue = max(dot(inNormal, sunlightDirection.xyz), 0.1f);
	
	vec4 color = (texture(materialTextures[baseTextureIndex], inUV) + vec4(0.055)) / vec4(1.055); 
	color.r = pow(color.r, 2.4);
	color.g = pow(color.g, 2.4);
	color.b = pow(color.b, 2.4);
	color.a = pow(color.a, 2.4);
	color *= inColor;
	vec4 ambient = color * ambientColor;

	outFragColor = vec4(color * lightValue * sunlightColor.w + ambient);
}
