#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec4 outColor;

void main() {
    if(scene.radianceTexture != -1){
        vec3 envColor = textureLod(texturesCube[scene.radianceTexture], inPos, 0.0).rgb;
        outColor = vec4(envColor, 1.0);
    }else{
        outColor = vec4(scene.ambientColorIntensity.xyz, 1.0);
    }
}
