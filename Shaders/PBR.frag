#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout( push_constant ) uniform PushConstants
{
	mat4 model;
    int colorId;
} constants;

void main() {
    outColor = texture(textures[constants.colorId], fragTexCoord);
}