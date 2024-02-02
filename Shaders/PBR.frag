#version 460

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout: require

#include "Common.glsl"

layout (constant_id = 0) const bool ALPHA_MASK = false;
layout (constant_id = 1) const float ALPHA_MASK_CUTOFF = 0.0;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec4 fragTangent;
layout(location = 4) flat in int fragColorId;
layout(location = 5) flat in int fragNormalId;
layout(location = 6) flat in int fragMRUId;
layout(location = 7) flat in int fragEmissiveId;
layout(location = 8) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

layout(std430, set = 0, binding = 2) readonly buffer Lights {
	int lightCount;
    Light lights[];
};

layout(set = 1, binding = 0) uniform sampler2D defaultTextures[4];
layout(set = 1, binding = 1) uniform sampler2D textures[];

layout(set = 2, binding = 0) uniform samplerCube irradianceTexture;
layout(set = 2, binding = 1) uniform samplerCube prefilterTexture;
layout(set = 2, binding = 2) uniform sampler2D brdfTexture;

// Convenience
mat3 constructTBN(vec3 normal, vec4 tangent);

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float costheta, vec3 F0);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

vec3 calculatePointLight(Light light, vec3 fragPos, vec3 viewPos, vec3 V, vec3 N, Material material);

void main() {
    vec4 color;
    if (fragColorId != -1) {
        color = texture(textures[fragColorId], fragTexCoord);
		if (ALPHA_MASK) {
			if (color.a < ALPHA_MASK_CUTOFF) {
				discard;
			}
		}
    } else {
        color = vec4(1, 1, 1, 1);
    }

    vec4 normal;
    if (fragNormalId != -1) {
        normal = texture(textures[fragNormalId], fragTexCoord);
    } else {
        normal = vec4(0.5, 0.5, 1, 0);
    }

    vec4 mru;
    if (fragMRUId != -1) {
        mru = texture(textures[fragMRUId], fragTexCoord);
    } else {
        mru = vec4(0, 0, 0, 0);
    }

	vec4 emissive;
	if (fragEmissiveId != -1) {
		emissive = texture(textures[fragEmissiveId], fragTexCoord);
	} else {
		emissive = vec4(0, 0, 0, 0);
	}

    // ----


    Material material;
    material.color = color;
	material.metallic = mru.r;
	material.roughness = mru.g;

    mat3 TBN = constructTBN(fragNormal, fragTangent);
	vec3 N = normalize(TBN * (2.0 * normal.rgb - 1.0));
	vec3 V = normalize(viewPos - fragPos);
	vec3 R = reflect(-V, N); 

    vec3 Lo = vec3(0.0);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, material.color.rgb, material.metallic);

	for (int i = 0; i < lightCount; i++)
	{
		Lo += calculatePointLight(lights[i], fragPos, viewPos, V, N, material);
	}

	vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, material.roughness);
	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - material.metallic;
	vec3 irradiance = texture(irradianceTexture, N).rgb;
	vec3 diffuse = irradiance * material.color.rgb;

	const float MAX_REFLECTION_LOD = 4.0;
	vec3 prefilteredColor = textureLod(prefilterTexture, R, material.roughness * MAX_REFLECTION_LOD).rgb;
	vec2 brdf = texture(brdfTexture, vec2(max(dot(N, V), 0.0), material.roughness)).rg;
	vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

	vec3 ambient = (kD * diffuse + specular);

    outColor = vec4(ambient + Lo + emissive.xyz, 1.0);
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
	return mat3(T,B, N);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - material.metallic;

	float NdotL = max(dot(N, L), 0.0);

	return (kD * material.color.xyz / PI + specular) * radiance * NdotL;
}