#version 450

#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;

layout (location = 3) in vec4 inAmbientColor;
layout (location = 4) in vec4 inSunlightDirection;
layout (location = 5) in vec4 inSunlightColor;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	float lightValue = max(dot(inNormal, inSunlightDirection.xyz), 0.1f);

	vec3 color = inColor * texture(colorTex,inUV).xyz;
	vec3 ambient = color *  inAmbientColor.xyz;

	outFragColor = vec4(color * lightValue *  inSunlightColor.w + ambient ,1.0f);
}
