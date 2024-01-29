#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inPos;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 baseColor = objects[pc.objectIndex].baseColor.rgb;
    vec3 lightDir = scene.lightDirection.xyz;
    vec3 directional = max(dot(lightDir, inNormal), 0.0) * scene.lightColorIntensity.rgb;
    vec3 ambient = scene.ambientColorIntensity.rgb;
    vec3 diffuse = baseColor * (directional + ambient);
    outColor = vec4(diffuse, 1.0);
}
