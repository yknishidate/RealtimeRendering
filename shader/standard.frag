#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inPos;
layout(location = 2) in vec4 inShadowCoord;
layout(location = 0) out vec4 outColor;

vec2 poissonDisk[4] = vec2[](
    vec2( -0.94201624, -0.39906216 ),
    vec2( 0.94558609, -0.76890725 ),
    vec2( -0.094184101, -0.92938870 ),
    vec2( 0.34495938, 0.29387760 )
);

void main() {
    vec3 directionalTerm = vec3(0.0);
    if(scene.existDirectionalLight == 1){
        vec3 lightDir = scene.lightDirection.xyz;
        float clampedCosTheta = max(dot(lightDir, inNormal), 0.0);
        directionalTerm = clampedCosTheta * scene.lightColorIntensity.rgb;

        if(scene.enableShadowMapping == 1){
            float bias = scene.shadowBias * tan(acos(clampedCosTheta));
            bias = clamp(clampedCosTheta, 0.0, scene.shadowBias * 2.0);
            
            #ifdef USE_PCF
                float rate = 0.0;
                for (int i = 0; i < 4; i++){
                    vec3 coord = inShadowCoord.xyz / inShadowCoord.w;
                    coord.xy += poissonDisk[i] / 1000.0;
                    coord.z -= bias;
                    rate += texture(shadowMap, coord).r * 0.25;
                }
                directionalTerm *= rate;
            #else
                if(texture(shadowMap, inShadowCoord.xy).r < inShadowCoord.z - bias){
                    directionalTerm = vec3(0.0);
                }
            #endif // USE_PCF
        }
    }
    
    vec3 ambientTerm = scene.ambientColorIntensity.rgb;

    vec3 baseColor = objects[pc.objectIndex].baseColor.rgb;
    vec3 diffuse = baseColor * (directionalTerm + ambientTerm);
    outColor = vec4(diffuse, 1.0);
}
