#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec4 fragTangent;
layout(location = 7) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D defaultTextures[4];
layout(set = 1, binding = 1) uniform sampler2D textures[];

layout( push_constant ) uniform PushConstants
{
	mat4 model;
    int colorId;
    int normalId;
    int mruId;
} constants;

const float PI = 3.14159265359;

struct Light {
	vec3 position;
};

struct Material {
	vec4 color;
	float roughness;
	float metallic;
};

// Convenience
mat3 constructTBN(vec3 normal, vec4 tangent);

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float costheta, vec3 F0);

vec3 calculatePointLight(Light light, vec3 fragPos, vec3 viewPos, vec3 V, vec3 N, Material material);

void main() {
    vec4 color;
    if (constants.colorId != -1) {
        color = texture(textures[constants.colorId], fragTexCoord);
    } else {
        color = vec4(1, 1, 1, 1);
    }

    vec4 normal;
    if (constants.normalId != -1) {
        normal = texture(textures[constants.normalId], fragTexCoord);
    } else {
        normal = vec4(0, 1, 0, 0);
    }

    vec4 mru;
    if (constants.mruId != -1) {
        mru = texture(textures[constants.mruId], fragTexCoord);
    } else {
        mru = vec4(0, 0, 0, 0);
    }

    // ----


    Material material;
    material.color = color;
	material.metallic = mru.r;
	material.roughness = mru.g;

    mat3 TBN = constructTBN(fragNormal, fragTangent);
	vec3 N = normalize(TBN * (2.0 * normal.rgb - 1.0));
	vec3 V = normalize(viewPos - fragPos);

    vec3 Lo = vec3(0.0);
	// Per light
	Light light;
	light.position = vec3(3, 1, 0);
	
	Lo += calculatePointLight(light, fragPos, viewPos, V, N, material);
	// ---- 

	vec3 ambient = vec3(0.03) * material.color.xyz;

    outColor = vec4(ambient + Lo, 1.0);
}


float DistributionGGX(vec3 N, vec3 H, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom/denom;
}
float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom/denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float costheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - costheta, 0.0, 1.0), 5.0);
}


mat3 constructTBN(vec3 normal, vec4 tangent) {
	vec3 N = normalize(normal);
	vec3 T = normalize(tangent.xyz);
	vec3 B = cross(T, N) * tangent.w;
	T = (T - dot(T, N) * N);
	return mat3(T, B, N);
}

vec3 calculatePointLight(Light light, vec3 fragPos, vec3 viewPos, vec3 V, vec3 N, Material material) {
	vec3 L = normalize(light.position - fragPos);
	float distance = length(light.position - fragPos);
	vec3 H = normalize(V + L);
	float attenuation = 1.0 / (distance * distance);
	vec3 radiance = vec3(23, 23, 23) * attenuation; // light colors

	// Cook BRDF 
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, material.color.xyz, material.metallic);

	float NDF = DistributionGGX(N, H, material.roughness);
	float G = GeometrySmith(N, V, L, material.roughness);
	vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

	vec3 numerator = NDF * G * F;
	float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
	vec3 specular = numerator/denominator;

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - material.metallic;

	float NdotL = max(dot(N, L), 0.0);

	return (kD * material.color.xyz / PI + specular) * radiance * NdotL;
}