#version 460

layout(binding = 2) uniform sampler2D colorImage;

layout(location = 0) out vec4 outColor;

void main(){
    vec2 uv = gl_FragCoord.xy;
    ourColor = vec4(uv, 0.0, 1.0);
}
