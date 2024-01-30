#version 460
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec3 color;
};

void main() {
    gl_Position = mvp * vec4(inPosition, 1);
    gl_PointSize = 5.0;
}
