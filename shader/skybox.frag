#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 yInverted = vec3(inPos.x, -inPos.y, inPos.z);
    // vec3 envColor = texture(texturesCube[scene.envMapIndex], yInverted).rgb;
    if(scene.envMapIndex != -1){
        vec3 envColor = textureLod(texturesCube[scene.envMapIndex], yInverted, 7.0).rgb;
        outColor = vec4(envColor, 1.0);
    }else{
        outColor = vec4(scene.ambientColorIntensity.xyz, 1.0);
    }
}
