#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inPos;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inShadowCoord;
layout(location = 0) out vec4 outColor;

vec2 poissonDisk[4] = vec2[](
    vec2( -0.94201624, -0.39906216 ),
    vec2( 0.94558609, -0.76890725 ),
    vec2( -0.094184101, -0.92938870 ),
    vec2( 0.34495938, 0.29387760 )
);

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 computeAmbientTerm(vec3 baseColor, float roughness, float metallic, vec3 occlusion,
                        vec3 N, vec3 V, vec3 R)
{
    // 非金属では固定値 0.04、金属では baseColor そのものとする
    // metallic で値をブレンドして F0 を決める
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    // -------------------- Diffuse --------------------
    // kd * (c/π) * ∫ Li (n・wi) dwi
    vec3 irradiance = vec3(0.0);
    if(scene.irradianceTexture != -1){
        // Irradiance に kD, c 以外の係数は含まれている
        irradiance = texture(texturesCube[scene.irradianceTexture], N).xyz;
    }else{
        // Solid color の場合、半球で積分すると値が π 倍になる
        // その後、π で割られるためキャンセルされる
        irradiance = scene.ambientColorIntensity.rgb;
    }
    vec3 diffuse = kD * baseColor * irradiance;

    // -------------------- Specular --------------------
    vec3 radiance = scene.ambientColorIntensity.rgb;
    if(scene.radianceTexture != -1){
        const float MAX_REFLECTION_LOD = 9.0;
        radiance = textureLod(texturesCube[scene.radianceTexture], R, roughness * MAX_REFLECTION_LOD).xyz;
    }

    // x: dot(N, V)
    // y: roughness
    vec2 envBRDF  = texture(brdfLutTexture, vec2(max(dot(N, V), 0.0), roughness)).xy;
    vec3 specular = radiance * (F * envBRDF.x + envBRDF.y);

    return (diffuse + specular) * occlusion;
}

// 法線 H をもつマイクロファセットの割合を返す
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / denom;
}

// NOTE: 視線方向または光源方向のいずれか一方から見たときのジオメトリ項
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness + 1.0;
    float k = (a * a) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

// NOTE:
// 二つの値を乗算した最終的なジオメトリ項
// cosThetaはジオメトリ項の中に含まれる
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 computeDirectionalTerm(vec3 baseColor, float metallic, float roughness, vec3 L, vec3 V, vec3 N)
{
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);

    vec3 H = normalize(L + V);
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;

    vec3 specular = numerator / denominator;
    vec3 diffuse = kD * baseColor / PI;

    vec3 Li = scene.lightColorIntensity.rgb;
    vec3 directionalTerm = (diffuse + specular) * Li * max(dot(N, L), 0.0);
    return directionalTerm;
}

void main() {
    vec3 N = inNormal;
    vec3 V = normalize(scene.cameraPos.xyz - inPos);
    vec3 L = scene.lightDirection.xyz;
    vec3 R = reflect(-V, N);
    
    // Load parameters
    vec3 baseColor = objects[pc.objectIndex].baseColor.rgb;
    vec3 emissive = objects[pc.objectIndex].emissive.rgb;
    vec3 occlusion = vec3(1.0);
    float metallic = objects[pc.objectIndex].metallic;
    float roughness = objects[pc.objectIndex].roughness;

    // Load textures
    // NOTE: テクスチャはリニア空間になっていないのでガンマ補正が必要
    int baseColorTexture = objects[pc.objectIndex].baseColorTextureIndex;
    int metallicRoughnessTexture = objects[pc.objectIndex].metallicRoughnessTextureIndex;
    int emissiveTextureIndex = objects[pc.objectIndex].emissiveTextureIndex;
    int occlusionTextureIndex = objects[pc.objectIndex].occlusionTextureIndex;
    if(baseColorTexture != -1){
        baseColor = texture(textures2D[baseColorTexture], inTexCoord).xyz;
        baseColor = gammaCorrect(baseColor, 2.2);
    }
    if(metallicRoughnessTexture != -1){
        vec3 metallicRoughness = texture(textures2D[metallicRoughnessTexture], inTexCoord).xyz;
        metallicRoughness = gammaCorrect(metallicRoughness, 2.2);
        roughness = metallicRoughness.y;
        metallic = metallicRoughness.z;
    }
    if(emissiveTextureIndex != -1){
        emissive = texture(textures2D[emissiveTextureIndex], inTexCoord).xyz;
        emissive = gammaCorrect(emissive, 2.2);
    }
    if(occlusionTextureIndex != -1){
        occlusion = texture(textures2D[occlusionTextureIndex], inTexCoord).xyz;
        occlusion = gammaCorrect(occlusion, 2.2);
    }

    vec3 directionalTerm = computeDirectionalTerm(baseColor, metallic, roughness, L, V, N);
    if(scene.existDirectionalLight == 1 && scene.enableShadowMapping == 1){
        float clampedCosTheta = max(dot(L, N), 0.0);
        float bias = scene.shadowBias * tan(acos(clampedCosTheta));
        bias = clamp(bias, 0.0, scene.shadowBias * 2.0);
        
        #ifdef USE_PCF
            float visibility = 0.0;
            for (int i = 0; i < 4; i++){
                vec3 coord = inShadowCoord.xyz / inShadowCoord.w;
                coord.xy += poissonDisk[i] / 1000.0;
                coord.z -= bias;
                visibility += texture(shadowMap, coord).r * 0.25;
            }
            directionalTerm *= visibility;
        #else
            if(texture(shadowMap, inShadowCoord.xy).r < inShadowCoord.z - bias){
                directionalTerm = vec3(0.0);
            }
        #endif // USE_PCF
    }

    vec3 ambientTerm = computeAmbientTerm(baseColor, roughness, metallic, occlusion, N, V, R);

    //outColor = vec4(directionalTerm, 1.0);
    outColor = vec4(emissive + ambientTerm + directionalTerm, 1.0);
}
