#version 460 core

#extension GL_EXT_multiview: require

layout (location = 0) in vec3 aPos;
layout (location = 0) out vec3 localPos;

layout(set = 0, binding = 0) uniform Bind0 {
    mat4 projection; 
    mat4 view[6];
};

//layout (push_constant) uniform Push {
//    int call;
//};

void main()
{
    localPos = aPos;  
    gl_Position =  projection * view[gl_ViewIndex] * vec4(localPos, 1.0);
}