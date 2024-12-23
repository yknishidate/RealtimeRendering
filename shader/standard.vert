#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outPos;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec4 outShadowCoord;
layout(location = 4) out mat3 outTBN;

void main() {
    mat4 modelMatrix = objects[pc.objectIndex].modelMatrix;
    mat3 normalMatrix = mat3(objects[pc.objectIndex].normalMatrix);

    mat4 cameraViewProj = scene.cameraViewProj;
    mat4 shadowViewProj = scene.shadowViewProj;

    vec4 worldPos = modelMatrix * vec4(inPosition, 1);
    gl_Position = cameraViewProj * worldPos;
    
    // for Normal mapping
    if(objects[pc.objectIndex].enableNormalMapping == 1){
        vec3 bitangent = cross(inNormal, inTangent.xyz) * inTangent.w;
        vec3 N = normalize(vec3(modelMatrix * vec4(inNormal, 0.0)));
	    vec3 T = normalize(vec3(modelMatrix * vec4(inTangent.xyz, 0.0)));
        vec3 B = normalize(vec3(modelMatrix * vec4(bitangent, 0.0)));
        outTBN = mat3(T, B, N);
    }

    outNormal = normalize(normalMatrix * inNormal);
    
    outPos = worldPos.xyz;
    outTexCoord = inTexCoord;

    vec4 shadowCoord = shadowViewProj * worldPos;
    shadowCoord.x = shadowCoord.x * +0.5 + 0.5;
    shadowCoord.y = shadowCoord.y * -0.5 + 0.5;
    outShadowCoord = shadowCoord;
}
