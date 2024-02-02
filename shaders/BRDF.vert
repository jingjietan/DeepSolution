#version 460 core

layout (location = 0) out vec2 fragCoords;

#include "Common.glsl"

void main()
{
	gl_Position = vec4(getQuadPosition(gl_VertexIndex), 0.0, 1.0);
	fragCoords = getQuadCoords(gl_Position.xy);
}