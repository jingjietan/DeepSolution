#version 460 core

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 fragUV;

layout(set = 0, binding = 0) uniform Bind0 {
    mat4 projection;
    mat4 view;
};

void main()
{
    fragUV = inPos;  
    // fragUV.xy *= -1.0;
    vec4 pos = projection * mat4(mat3(view)) * vec4(inPos, 1.0);
    gl_Position = pos.xyww;
}