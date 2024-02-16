#version 460
#include "standard.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inPos;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inShadowCoord;
layout(location = 4) in mat3 inTBN;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

vec2 poissonDisk[4] = vec2[](
    vec2( -0.94201624,  -0.39906216 ),
    vec2(  0.94558609,  -0.76890725 ),
    vec2( -0.094184101, -0.92938870 ),
    vec2(  0.34495938,   0.29387760 )
);

void loadMaterial(out vec4 baseColor, out vec3 normal, out vec3 emissive, out vec3 occlusion, out float metallic, out float roughness)
{
    // Load parameters
    baseColor = objects[pc.objectIndex].baseColor;
    emissive = objects[pc.objectIndex].emissive.rgb;
    occlusion = vec3(1.0);
    metallic = objects[pc.objectIndex].metallic;
    roughness = objects[pc.objectIndex].roughness;
    normal = normalize(inNormal);

    // Load textures
    // NOTE:
    // テクスチャはリニア空間になっていないのでガンマ補正が必要
    //   baseColor:         sRGB
    //   emissive:          sRGB
    //   mettalicRoughness: linear
    //   normal:            linear
    //   occlusion:         不明。おそらく linear
    // WARN: 厳密にはsRGBtoLinearとGamma2.2の処理は異なる
    // TODO: 
    // そもそもシェーダで変換するのではなく、vk::ImageのFormatで適切にsRGBかUnormかを指定し、
    // GPU側で補正してもらうべき。
    int baseColorTexture = objects[pc.objectIndex].baseColorTextureIndex;
    int metallicRoughnessTexture = objects[pc.objectIndex].metallicRoughnessTextureIndex;
    int emissiveTextureIndex = objects[pc.objectIndex].emissiveTextureIndex;
    int occlusionTextureIndex = objects[pc.objectIndex].occlusionTextureIndex;
    int normalTextureIndex = objects[pc.objectIndex].normalTextureIndex;
    int enableNormalMapping = objects[pc.objectIndex].enableNormalMapping;
    if(baseColorTexture != -1){
        baseColor = texture(textures2D[baseColorTexture], inTexCoord);
        baseColor = gammaCorrect(baseColor, 2.2);
    }
    if(metallicRoughnessTexture != -1){
        vec3 metallicRoughness = texture(textures2D[metallicRoughnessTexture], inTexCoord).xyz;
        roughness = metallicRoughness.y;
        metallic = metallicRoughness.z;
    }
    if(emissiveTextureIndex != -1){
        emissive = texture(textures2D[emissiveTextureIndex], inTexCoord).xyz;
        emissive = gammaCorrect(emissive, 2.2);
    }
    if(occlusionTextureIndex != -1){
        occlusion = texture(textures2D[occlusionTextureIndex], inTexCoord).xyz;
    }
    if(enableNormalMapping == 1 && normalTextureIndex != -1){
        // TODO:
        // normal texture が含まれていても、Tangent が含まれていないデータがある。
        // その場合は、CPU側で MikkTSpace を使って事前に Tangent を計算するべき。
        // MikkTSpace は vcpkg にも含まれている。
        normal = texture(textures2D[normalTextureIndex], inTexCoord).xyz;
        normal = normalize(normal * 2.0 - 1.0); // remap: [0, 1] -> [-1, 1]
        normal = normalize(inTBN * normal);
    }

    // Roughness は 0.0 だと問題が起きるため最小値を設定
    // Sampler が ClampToEdge 以外だと 1.0 でも問題が起きるため設定に注意
    roughness = max(roughness, 0.01);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 computeAmbientTerm(vec3 baseColor, float roughness, vec3 occlusion,
                        vec3 F, vec3 kD,
                        vec3 N, vec3 V, vec3 R)
{
    // Diffuse
    // kd * (c/π) * ∫ Li (n・wi) dwi
    float intensity = scene.ambientColorIntensity.w;
    vec3 irradiance = vec3(0.0);
    if(scene.irradianceTexture != -1){
        // Irradiance に kD, c 以外の係数は含まれている
        irradiance = texture(texturesCube[scene.irradianceTexture], N).xyz * intensity;
    }else{
        // Solid color の場合、半球で積分すると値が π 倍になる
        // その後、π で割られるためキャンセルされる
        irradiance = scene.ambientColorIntensity.rgb * intensity;
    }
    vec3 diffuse = kD * baseColor * irradiance;

    // Specular
    vec3 radiance = scene.ambientColorIntensity.rgb;
    if(scene.radianceTexture != -1){
        const float MAX_REFLECTION_LOD = 9.0;
        radiance = textureLod(texturesCube[scene.radianceTexture], R, roughness * MAX_REFLECTION_LOD).xyz;
    }
    radiance *= intensity;

    // x: dot(N, V)
    // y: roughness
    vec2 envBRDF  = texture(brdfLutTexture, vec2(max(dot(N, V), 0.0), roughness)).xy;
    vec3 specular = radiance * (F * envBRDF.x + envBRDF.y);
    return (diffuse + specular) * occlusion;
}

// 法線 H をもつマイクロファセットの割合を返す
float distributionGGX(vec3 N, vec3 H, float roughness)
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
float geometrySchlickGGX(float NdotV, float roughness)
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
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = geometrySchlickGGX(NdotV, roughness);
    float ggx2 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 computeDirectionalTerm(vec3 baseColor, float roughness,
                            vec3 F, vec3 kD, 
                            vec3 L, vec3 V, vec3 N)
{
    vec3 H = normalize(L + V);
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;

    vec3 specular = numerator / denominator;
    vec3 diffuse = kD * baseColor / PI;

    vec3 Li = scene.lightColorIntensity.xyz * scene.lightColorIntensity.w;
    vec3 directionalTerm = (diffuse + specular) * Li * max(dot(N, L), 0.0);
    return directionalTerm;
}

float computeDirectionalVisibility(vec3 N, vec3 L)
{
    if(scene.existDirectionalLight == 0 || scene.enableShadowMapping == 0){
        return 1.0;
    }

    float NdotL = max(dot(N, L), 0.0);
    float bias = scene.shadowBias * tan(acos(NdotL));
    bias = clamp(bias, 0.0, scene.shadowBias * 2.0);
        
    #ifdef USE_PCF
        float visibility = 0.0;
        for (int i = 0; i < 4; i++){
            // NOTE: shadow map は vec3 でサンプリングする
            // TODO: bias は z に反映させているが Vulkan の設定でできるはず
            vec3 coord = inShadowCoord.xyz / inShadowCoord.w;
            coord.xy += poissonDisk[i] / 1000.0;
            coord.z -= bias;
            visibility += texture(shadowMap, coord).r * 0.25;
        }
        return visibility;
    #else
        if(texture(shadowMap, inShadowCoord.xy).r < inShadowCoord.z - bias){
            return 0.0;
        }
    #endif // USE_PCF
}

void main() {
    // Load material
    vec4 baseColor;
    vec3 normal, emissive, occlusion;
    float metallic, roughness;
    loadMaterial(baseColor, normal, emissive, occlusion, metallic, roughness);
    if(baseColor.w < 1.0){
        discard;
    }

    // TODO: normal mapを使うかどうかをObjectDataに入れる
    vec3 N = normal;
    vec3 V = normalize(scene.cameraPos.xyz - inPos);
    vec3 L = scene.lightDirection.xyz;
    vec3 R = reflect(-V, N);
    vec3 H = normalize(L + V);
    
    // Common values
    // F0: 非金属では固定値 0.04、金属では baseColor そのものとする
    //     metallic で値をブレンドして F0 を決める
    vec3 F0 = mix(vec3(0.04), baseColor.xyz, metallic);
    vec3 F = fresnelSchlickRoughness(max(dot(V, H), 0.0), F0, roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    // Directional light
    vec3 directionalTerm = computeDirectionalTerm(baseColor.xyz, roughness, F, kD, L, V, N);
    float visibility = computeDirectionalVisibility(N, L);
    directionalTerm *= visibility;

    // Ambient light
    vec3 ambientTerm = computeAmbientTerm(baseColor.xyz, roughness, occlusion, F, kD, N, V, R);
    
    // Final color
    outColor = vec4(emissive + ambientTerm + directionalTerm, 1.0);
    if(scene.enableSSR == 1){
        outNormal = vec4(N, 1.0);
    }
}
