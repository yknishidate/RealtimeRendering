#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;
layout(location = 0) out vec3 outPos;

void main() {
    outPos = inPosition;

    // remove translation from the view matrix
    mat4 rotView = mat4(mat3(scene.cameraView));
    vec4 clipPos = scene.cameraProj * rotView * vec4(inPosition, 1.0);

    // depth = 1.0
    gl_Position = clipPos.xyww;
}
