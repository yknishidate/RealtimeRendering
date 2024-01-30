#version 460
layout(location = 0) out vec4 outColor;
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec3 color;
};

void main() {
    outColor = vec4(color, 1.0);
}
