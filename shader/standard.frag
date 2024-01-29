#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inPos;
layout(location = 2) in vec3 inShadowCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 directionalTerm = vec3(0.0);
    if(scene.existDirectionalLight == 1){
        vec3 lightDir = scene.lightDirection.xyz;
        directionalTerm = max(dot(lightDir, inNormal), 0.0) * scene.lightColorIntensity.rgb;

        if(scene.enableShadowMapping == 1){
            if(texture(shadowMap, inShadowCoord.xy).r < inShadowCoord.z - 0.001){
                directionalTerm *= 0.5;
            }
        }
    }
    
    vec3 ambientTerm = scene.ambientColorIntensity.rgb;

    vec3 baseColor = objects[pc.objectIndex].baseColor.rgb;
    vec3 diffuse = baseColor * (directionalTerm + ambientTerm);
    outColor = vec4(diffuse, 1.0);
}
