#version 450

#include "Common.glsl"

layout(location = 0) out vec2 fragTexCoord;

void main() {
    gl_Position = vec4(getQuadPosition(gl_VertexIndex), 0.0, 1.0);
    fragTexCoord = getQuadCoords(gl_Position.xy);
}