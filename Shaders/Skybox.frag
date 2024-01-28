#version 460 core

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragUV;

layout(set = 0, binding = 1) uniform samplerCube cubemap;


void main()
{		
    outColor = texture(cubemap, fragUV);
}