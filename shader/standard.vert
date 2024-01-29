#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outPos;
layout(location = 2) out vec4 outShadowCoord;

void main() {
    mat4 modelMatrix = objects[pc.objectIndex].modelMatrix;
    mat3 normalMatrix = mat3(objects[pc.objectIndex].normalMatrix);

    mat4 cameraViewProj = scene.cameraViewProj;
    mat4 shadowViewProj = scene.shadowViewProj;

    vec4 worldPos = modelMatrix * vec4(inPosition, 1);
    gl_Position = cameraViewProj * worldPos;
    
    outNormal = normalize(normalMatrix * inNormal);
    
    outPos = inPosition;

    vec4 shadowCoord = shadowViewProj * worldPos;
    shadowCoord.x = shadowCoord.x * +0.5 + 0.5;
    shadowCoord.y = shadowCoord.y * -0.5 + 0.5;
    outShadowCoord = shadowCoord;
}
