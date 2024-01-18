#version 450

layout(binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.projection * ubo.view * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}