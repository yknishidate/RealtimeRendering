#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

void main() {
    mat4 model = objects[pc.objectIndex].modelMatrix;
    mat4 viewProj = scene.shadowViewProj;
    gl_Position = viewProj * model * vec4(inPosition, 1);
}
