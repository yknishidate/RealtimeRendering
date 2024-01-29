#ifdef __cplusplus
using namespace glm;
#endif

// --------------------------
// ---------- Share ---------
struct ShadowMapConstants{
    mat4 mvp;
};

// --------------------------
// ---------- C++ -----------
#ifdef __cplusplus

#else

// --------------------------
// ---------- GLSL ----------

layout(push_constant) uniform PushConstants {
    ShadowMapConstants pc;
};

#endif
