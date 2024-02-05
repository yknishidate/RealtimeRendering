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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = inNormal;
    vec3 V = normalize(inPos - scene.cameraPos.xyz);
    vec3 L = scene.lightDirection.xyz;

    vec3 directionalTerm = vec3(0.0);
    if(scene.existDirectionalLight == 1){
        float clampedCosTheta = max(dot(L, N), 0.0);
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
    
    // Parameters
    vec3 baseColor = objects[pc.objectIndex].baseColor.rgb;
    float roughness = objects[pc.objectIndex].roughness;
    float metallic = objects[pc.objectIndex].metallic;

    vec3 ambientTerm = scene.ambientColorIntensity.rgb;
    if(scene.irradianceTexture != -1){
        // diffuse には irradiance として LoD=10.0を使用
        // 正確には最大フィルタリングと irradiance は異なるため修正
        ambientTerm = textureLod(texturesCube[scene.irradianceTexture], N, 10.0).xyz;
    }

    // 非金属では固定値 0.04、金属では baseColor そのものとする
    // metallic で値をブレンドして F0 を決める
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness); 
    vec3 kD = 1.0 - kS;

    vec3 diffuse = kD * baseColor * (directionalTerm + ambientTerm);
    vec3 specular = kS * vec3(0.0);
    outColor = vec4(diffuse + specular, 1.0);
}
