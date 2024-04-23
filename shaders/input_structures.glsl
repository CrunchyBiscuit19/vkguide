layout(set = 0, binding = 0) uniform GLTFMaterialData{   
	vec4 colorFactors;
	vec4 metalRoughFactors;
} materialData;

layout(set = 0, binding = 1) uniform sampler2D colorTex;
layout(set = 0, binding = 2) uniform sampler2D metalRoughTex;
