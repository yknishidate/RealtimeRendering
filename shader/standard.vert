#version 460
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec3 outNormal;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    mat4 model;
    vec3 color;
};

void main() {
    gl_Position = viewProj * model * vec4(inPosition, 1);
    outNormal = inNormal;
}
