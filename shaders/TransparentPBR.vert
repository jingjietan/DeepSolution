#version 450

// Copied from PBR.vert

layout(set = 0, binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out float depth;

layout( push_constant ) uniform PushConstants
{
	mat4 model;
    uint colorId;
} constants;

void main() {
    gl_Position = ubo.projection * ubo.view * constants.model * vec4(inPosition, 1.0);
    depth = (ubo.view * constants.model * vec4(inPosition, 1.0)).z;
    fragTexCoord = inTexCoord;
}