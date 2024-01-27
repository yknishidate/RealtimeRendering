#ifdef __cplusplus
struct StandardConstants{
    int objectIndex;
};

struct SceneData{
    glm::mat4 viewProj; // 64 bytes

    // Directional light
    glm::vec4 lightDirection; // 16 bytes
    glm::vec4 lightColorIntensity; // 16 bytes

    // Ambient light
    glm::vec4 ambientColorIntensity; // 16 bytes
};

struct ObjectData{
    // Transform
    glm::mat4 transformMatrix; // 64 bytes
    glm::mat4 normalMatrix;    // 64 bytes

    // Material
    glm::vec4 baseColor;       // 16 bytes
};

#else

// --------------------------
// ---------- GLSL ----------

#extension  GL_EXT_nonuniform_qualifier : enable

struct ObjectData{
    // Transform
    mat4 transformMatrix;
    mat4 normalMatrix;

    // Material
    vec4 baseColor;
};

layout(push_constant) uniform PushConstants {
    int objectIndex;
};

layout(binding = 1) buffer ObjectBuffer {
    ObjectData objects[];
};

layout(binding = 0) uniform Scene {
    // Camera
    mat4 viewProj;

    // Directional light
    vec4 lightDirection;
    vec4 lightColorIntensity; // intensity * color

    // Ambient light
    vec4 ambientColorIntensity;
} scene;

#endif
