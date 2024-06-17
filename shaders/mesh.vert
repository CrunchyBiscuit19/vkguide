#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;

layout (location = 2) out vec4 outAmbientColor;
layout (location = 3) out vec4 outSunlightDirection;
layout (location = 4) out vec4 outSunlightColor;

// Buffer device addresses
struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
}; 
struct Instance {
	mat4 translation;
    mat4 rotation;
    mat4 scale;
};
struct SceneData {   
	mat4 view;
	mat4 proj;
	vec4 ambientColor;
	vec4 sunlightDirection; // w for sun power
	vec4 sunlightColor;
};
struct Material {
    vec4 baseFactor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    vec2 padding;
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
layout(buffer_reference, std430) readonly buffer MaterialBuffer{ 
	Material material;
};

// Push constants block
layout( push_constant, std430 ) uniform PushConstants
{
	VertexBuffer vertexBuffer;
	InstanceBuffer instanceBuffer;
	SceneBuffer sceneBuffer;
	MaterialBuffer materialBuffer; 
} constants;

void main() 
{

	Vertex v = constants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	mat4 translation = constants.instanceBuffer.instances[gl_InstanceIndex].translation;
	mat4 rotation = constants.instanceBuffer.instances[gl_InstanceIndex].rotation;
	mat4 scale = constants.instanceBuffer.instances[gl_InstanceIndex].scale;
	mat4 proj = constants.sceneBuffer.sceneData.proj; 
	mat4 view = constants.sceneBuffer.sceneData.view; 

	mat4 renderMatrix = mat4(1.0) * mat4(1.0) * mat4(1.0);
	gl_Position =  proj * view * renderMatrix * position; // PVM matrices

	outNormal = mat3(transpose(inverse(renderMatrix))) * v.normal;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;

	outAmbientColor = constants.sceneBuffer.sceneData.ambientColor;
	outSunlightColor = constants.sceneBuffer.sceneData.sunlightColor;
	outSunlightDirection = constants.sceneBuffer.sceneData.sunlightDirection;
}