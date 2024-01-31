// --------------------------
// ---------- Share ---------
struct StandardConstants{
#ifdef __cplusplus
    int objectIndex = 0;
    int _dummy0 = 0;
    int _dummy1 = 0;
    int _dummy2 = 0;
    glm::vec2 screenResolution{0.0f, 0.0f};
#else
    int objectIndex;
    int _dummy0;
    int _dummy1;
    int _dummy2;
    vec2 screenResolution;
#endif
};

struct ObjectData{
#ifdef __cplusplus
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::vec4 baseColor{1.0f};
#else
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec4 baseColor;
#endif
};

struct SceneData{
#ifdef __cplusplus
    glm::mat4 cameraViewProj{1.0f};
    glm::mat4 shadowViewProj{1.0f};
    glm::vec4 lightDirection{0.0f};
    glm::vec4 lightColorIntensity{0.0f};
    glm::vec4 ambientColorIntensity{0.0f};
    int existDirectionalLight;
    int enableShadowMapping;
    float shadowBias = 0.005f;
#else
    mat4 cameraViewProj;
    mat4 shadowViewProj;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 ambientColorIntensity;
    int existDirectionalLight;
    int enableShadowMapping;
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

//layout(binding = 3) uniform sampler2D baseColorImage;

#endif
