#version 460 core

#extension GL_EXT_multiview: require

layout (location = 0) in vec3 inPos;
layout (location = 0) out vec3 fragPos;

layout(binding = 0) uniform Bind0 {
    mat4 projection; 
    mat4 view[6];
};

void main()
{
    fragPos = inPos;  
    gl_Position =  projection * view[gl_ViewIndex] * vec4(fragPos, 1.0);
}