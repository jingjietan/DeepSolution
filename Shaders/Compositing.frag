#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D accum;
layout(set = 0, binding = 1) uniform sampler2D reveal;

void main() {
    vec4 accumC = texture(accum, fragTexCoord);
    float revealC = texture(reveal, fragTexCoord).r;
    outColor = vec4(accumC.rgb / max(accumC.a, 1e-5), revealC);
}