// --------------------------
// ---------- Share ---------
struct StandardConstants{
#ifdef __cplusplus
    int objectIndex = 0;
#else
    int objectIndex;
#endif
};

struct ObjectData{
#ifdef __cplusplus
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::vec4 baseColor{1.0f};
    glm::vec4 emissive{0.0f};
    float metallic{0.0f};
    float roughness{1.0f};
    float ior{1.5f};
    int baseColorTextureIndex{-1};
    int metallicRoughnessTextureIndex{-1};
    int normalTextureIndex{-1};
    int occlusionTextureIndex{-1};
    int emissiveTextureIndex{-1};
#else
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec4 baseColor;
    vec4 emissive;
    float metallic;
    float roughness;
    float ior;
    int baseColorTextureIndex;
    int metallicRoughnessTextureIndex;
    int normalTextureIndex;
    int occlusionTextureIndex;
    int emissiveTextureIndex;
#endif
};

struct SceneData{
#ifdef __cplusplus
    glm::mat4 cameraView{1.0f};
    glm::mat4 cameraProj{1.0f};
    glm::mat4 cameraViewProj{1.0f};
    glm::mat4 shadowViewProj{1.0f};
    glm::vec4 lightDirection{0.0f};
    glm::vec4 lightColorIntensity{0.0f};
    glm::vec4 ambientColorIntensity{0.0f};
    glm::vec4 cameraPos{0.0f};
    glm::vec2 screenResolution{0.0f, 0.0f};
    int existDirectionalLight;
    int enableShadowMapping;
    int enableFXAA = 1;
    int irradianceTexture = -1;
    int radianceTexture = -1;
    float shadowBias = 0.005f;
#else
    mat4 cameraView;
    mat4 cameraProj;
    mat4 cameraViewProj;
    mat4 shadowViewProj;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 ambientColorIntensity;
    vec4 cameraPos;
    vec2 screenResolution;
    int existDirectionalLight;
    int enableShadowMapping;
    int enableFXAA;
    int irradianceTexture;
    int radianceTexture;
    float shadowBias;
#endif
};

// --------------------------
// ---------- C++ -----------
#ifdef __cplusplus

#else

// --------------------------
// ---------- GLSL ----------

#extension  GL_EXT_nonuniform_qualifier : enable
//#extension GL_EXT_debug_printf : enable

const float PI = 3.14159265359;

layout(push_constant) uniform PushConstants {
    StandardConstants pc;
};

layout(binding = 1) buffer ObjectBuffer {
    ObjectData objects[];
};

layout(binding = 0) uniform SceneBuffer {
    SceneData scene;
};

#define USE_PCF
#ifdef USE_PCF
layout(binding = 2) uniform sampler2DShadow shadowMap;
#else
layout(binding = 2) uniform sampler2D shadowMap;
#endif // USE_PCF

layout(binding = 3) uniform sampler2D baseColorImage;

layout(binding = 4) uniform sampler2D textures2D[];
layout(binding = 5) uniform samplerCube texturesCube[];
layout(binding = 6) uniform sampler2D brdfLutTexture;

#endif
