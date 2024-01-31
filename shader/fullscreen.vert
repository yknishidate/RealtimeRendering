#version 460

void main() {
    vec4 positions[3] = vec4[3](
        vec4(0, 0, 0, 1),
        vec4(0, 3, 0, 1),
        vec4(3, 0, 0, 1)
    );
    gl_Position = positions[gl_VertexID];
}
