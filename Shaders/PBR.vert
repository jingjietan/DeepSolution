#version 450

#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex 
{
	//float px, py, pz;
	//float nx, ny, nz;
	vec3 position;
	vec3 normal;
	vec4 tangent;
	vec2 texcoord;
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer Vertices
{
    Vertex vertices[];
};

layout(set = 0, binding = 0) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
	vec3 viewPos;
} ubo;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragTangent;
layout(location = 7) out vec3 viewPos;

layout( push_constant ) uniform PushConstants
{
	mat4 model;
	Vertices vertexBuffer;
    int colorId;
} constants;

void main() {
	restrict Vertex vertex = constants.vertexBuffer.vertices[gl_VertexIndex];
	vec3 inPosition = vertex.position;
	vec3 inNormal = vertex.normal; 
	vec4 inTangent = vertex.tangent;
	vec2 inTexCoord = vertex.texcoord;

	// vec4 pos = constants.model * vec4(inPosition, 1.0);
	fragPos = inPosition.xyz;
	fragTexCoord = inTexCoord;
	viewPos = ubo.viewPos;

	fragNormal = normalize(mat3(constants.model) * inNormal);
	fragTangent = vec4(normalize(inTangent.xyz), inTangent.w);

    gl_Position = ubo.projection * ubo.view * constants.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}