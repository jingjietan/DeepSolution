#version 450

layout(set = 0, binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
	vec3 viewPos;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragTangent;
layout(location = 7) out vec3 viewPos;

layout( push_constant ) uniform PushConstants
{
	mat4 model;
    int colorId;
} constants;

void main() {
	// vec4 pos = constants.model * vec4(inPosition, 1.0);
	fragPos = inPosition.xyz;
	fragTexCoord = inTexCoord;
	viewPos = ubo.viewPos;

	fragNormal = normalize(mat3(constants.model) * inNormal);
	fragTangent = vec4(normalize(inTangent.xyz), inTangent.w);

    gl_Position = ubo.projection * ubo.view * constants.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}