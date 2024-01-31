struct DrawData 
{
	mat4 model;
    int colorId;
	int normalId;
    int mruId;
};

struct Light 
{
	vec3 position;
};

struct Material 
{
	vec4 color;
	float roughness;
	float metallic;
};

const float PI = 3.14159265359;

#define GLOBAL_UNIFORM_BINDING set = 0, binding = 0

vec2 vertices[3] = {vec2(-1,-1), vec2(3,-1), vec2(-1, 3)};

vec2 getQuadPosition(uint index) {
	return vertices[index];
}

vec2 getQuadCoords(vec2 position) {
	return 0.5 * position + vec2(0.5);
}