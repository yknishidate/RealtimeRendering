// --------------------------
// ---------- Share ---------
struct StandardConstants{
    int objectIndex;
};

struct ObjectData{
#ifdef __cplusplus
    glm::mat4 transformMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::vec4 baseColor{1.0f};
#else
    mat4 transformMatrix;
    mat4 normalMatrix;
    vec4 baseColor;
#endif
};

struct SceneData{
#ifdef __cplusplus
    glm::mat4 viewProj{1.0f};
    glm::vec4 lightDirection{0.0f};
    glm::vec4 lightColorIntensity{0.0f};
    glm::vec4 ambientColorIntensity{0.0f};
#else
    mat4 viewProj;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 ambientColorIntensity;
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

#endif
