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