// --------------------------
// ---------- Share ---------
struct StandardConstants{
    int objectIndex;
};

struct ObjectData{
#ifdef __cplusplus
    glm::mat4 mvpMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::mat4 biasedShadowMatrix{1.0f};
    glm::vec4 baseColor{1.0f};
#else
    mat4 mvpMatrix;
    mat4 normalMatrix;
    mat4 biasedShadowMatrix;
    vec4 baseColor;
#endif
};

struct SceneData{
#ifdef __cplusplus
    glm::vec4 lightDirection{0.0f};
    glm::vec4 lightColorIntensity{0.0f};
    glm::vec4 ambientColorIntensity{0.0f};
    int existDirectionalLight;
    int enableShadowMapping;
#else
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 ambientColorIntensity;
    int existDirectionalLight;
    int enableShadowMapping;
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

layout(binding = 2) uniform sampler2D shadowMap;
#endif
