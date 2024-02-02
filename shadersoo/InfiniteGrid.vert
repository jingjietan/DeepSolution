#version 460

#include "Common.glsl"

layout(GLOBAL_UNIFORM_BINDING) uniform GlobalUniform {
    mat4 view;
    mat4 projection;
	vec3 viewPos;
} ubo;

layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;
layout(location = 2) out mat4 fragView;
layout(location = 6) out mat4 fragProjection;
layout(location = 10) out float near;
layout(location = 11) out float far;

vec3 gridPlane[6] = vec3[](
    vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
    vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0)
);

vec3 unprojectPoint(float x, float y, float z, mat4 view, mat4 projection) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(projection);
    vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec3 p = gridPlane[gl_VertexIndex].xyz;
    nearPoint = unprojectPoint(p.x, p.y, 0.0, ubo.view, ubo.projection).xyz;
    farPoint = unprojectPoint(p.x, p.y, 1.0, ubo.view, ubo.projection).xyz;

    fragView = ubo.view;
    fragProjection = ubo.projection;

    near = 0.01;
    far = 100;

    gl_Position = vec4(p, 1.0);
    // gl_Position.y = -gl_Position.y;
}