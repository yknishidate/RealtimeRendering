#version 460
#include "shadow_map.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1);
}
