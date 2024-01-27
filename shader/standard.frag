#version 460
layout(location = 0) in vec3 inNormal;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    mat4 model;
    vec3 color;
};

void main() {
    vec3 lightDir = normalize(vec3(1, 2, -3));
    vec3 diffuse = color * (dot(lightDir, inNormal) * 0.5 + 0.5);
    outColor = vec4(diffuse, 1.0);
}
