#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec4 outColor;

void main() {
    float intensity = scene.ambientColorIntensity.w;
    if(scene.radianceTexture != -1){
        vec3 envColor = textureLod(texturesCube[scene.radianceTexture], inPos, 0.0).rgb;
        outColor = vec4(envColor * intensity, 1.0);
    }else{
        vec3 envColor = scene.ambientColorIntensity.xyz;
        outColor = vec4(envColor * intensity, 1.0);
    }
}
