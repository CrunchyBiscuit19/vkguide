#extension GL_EXT_buffer_reference : require

layout (set = 0, binding = 0) uniform sampler2D materialTextures[];

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
	Material materials[];
};
layout(buffer_reference, std430) readonly buffer TransformBuffer{ 
	mat4 transforms[];
};

// Push constants block
layout( push_constant, std430 ) uniform PushConstants
{
	VertexBuffer vertexBuffer;
	InstanceBuffer instanceBuffer;
	SceneBuffer sceneBuffer;
	MaterialBuffer materialBuffer; 
	TransformBuffer transformBuffer;
	uint materialIndex;
	uint meshIndex;
} constants;