#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outPos;

void main() {
    mat4 transformMatrix = objects[objectIndex].transformMatrix;
    mat3 normalMatrix = mat3(objects[objectIndex].normalMatrix);
    gl_Position = scene.viewProj * transformMatrix * vec4(inPosition, 1);
    outNormal = normalize(normalMatrix * inNormal);
    outPos = inPosition;
}
