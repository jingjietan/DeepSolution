#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D defaultTextures[4];
layout(set = 1, binding = 1) uniform sampler2D textures[];

layout( push_constant ) uniform PushConstants
{
	mat4 model;
    int colorId;
} constants;

void main() {
    vec4 color;
    if (constants.colorId != -1) {
        color = texture(textures[constants.colorId], fragTexCoord);
    } else {
        color = texture(defaultTextures[0], fragTexCoord);
    }
    outColor = color;
}