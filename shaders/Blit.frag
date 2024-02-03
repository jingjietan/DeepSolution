#version 460 core

layout (location = 0) out vec4 FragColor;
layout (location = 0) in vec2 fragCoords;

layout(set = 0, binding = 0) uniform sampler2D hdrImage;

void main() 
{
	FragColor = texture(hdrImage, fragCoords);
}