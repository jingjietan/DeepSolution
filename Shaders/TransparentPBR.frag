#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in float depth;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outReveal;

layout(set = 1, binding = 1) uniform sampler2D textures[];

layout( push_constant ) uniform PushConstants
{
	mat4 model;
    int colorId;
} constants;

void main() {
    // Shaded
    vec4 color = texture(textures[constants.colorId], fragTexCoord);

    // ----
    color.rgb *= color.a;

    const float depthZ = -depth * 10.0f;

    float alphaWeight = 
      min(1.0, max(max(color.r, color.g), max(color.b, color.a)) * 40.0 + 0.01);
      alphaWeight *= alphaWeight;

    const float distWeight = clamp(0.03 / (1e-5 + pow(depthZ / 200, 4.0)), 1e-2, 3e3);
    const float weight = alphaWeight * distWeight;

    outColor = color * weight;
    outReveal = color.a;
}