#version 460

#extension GL_EXT_scalar_block_layout: require

#include "Common.glsl"

layout(GLOBAL_UNIFORM_BINDING) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
	vec3 viewPos;
} ubo;

layout(scalar, set = 0, binding = 1) buffer PerMeshDraw {
    DrawData drawDatas[];
};

layout( push_constant ) uniform DrawIdOffset  {
	uint drawOffset;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragTangent;
layout(location = 4) out int fragColorId;
layout(location = 5) out int fragNormalId;
layout(location = 6) out int fragMRUId;
layout(location = 7) out int fragEmissiveId;
layout(location = 8) out vec3 viewPos;

void main() {
	// vec4 pos = constants.model * vec4(inPosition, 1.0);
	DrawData drawData = drawDatas[gl_DrawID + drawOffset];

	fragPos = vec3(drawData.model * vec4(inPosition.xyz, 1.0));
	fragTexCoord = inTexCoord;
	viewPos = ubo.viewPos;

	fragNormal = normalize(transpose(inverse(mat3(drawData.model))) * inNormal);
	fragTangent = vec4(normalize(inTangent.xyz), inTangent.w);

    gl_Position = ubo.projection * ubo.view * vec4(fragPos, 1.0);
    fragTexCoord = inTexCoord;

	fragColorId = drawData.colorId;
	fragNormalId = drawData.normalId;
	fragMRUId = drawData.mruId;
	fragEmissiveId = drawData.emissiveId;
}