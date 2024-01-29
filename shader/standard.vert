#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outPos;
layout(location = 2) out vec3 outShadowCoord;

void main() {
    mat4 mvpMatrix = objects[pc.objectIndex].mvpMatrix;
    mat3 normalMatrix = mat3(objects[pc.objectIndex].normalMatrix);
    mat4 shadowMatrix = objects[pc.objectIndex].biasedShadowMatrix;
    gl_Position = mvpMatrix * vec4(inPosition, 1);
    outNormal = normalize(normalMatrix * inNormal);
    outPos = inPosition;
    outShadowCoord = vec3(shadowMatrix * vec4(inPosition, 1));
}
