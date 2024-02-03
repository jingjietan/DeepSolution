#version 460 core

layout(binding = 0) uniform sampler2D hdrImage;
layout(binding = 1) uniform sampler2D bloomImage;

layout (location = 0) in vec2 fragCoord;
layout (location = 0) out vec3 FragColor;

void main()
{
	vec3 hdrColor = texture(hdrImage, fragCoord).rgb;
	vec3 bloomColor = texture(bloomImage, fragCoord).rgb;
	FragColor = mix(hdrColor, bloomColor, 0.04f);
}