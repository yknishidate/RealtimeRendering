#version 460
#include "standard.glsl"

layout(location = 0) out vec4 outColor;

void main(){
    vec2 resolution = scene.screenResolution;
    vec2 inverseVP = 1.0 / resolution;
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord * inverseVP;

    vec3 color = texture(baseColorImage, uv).xyz;
    float depth = texture(depthImage, uv).x;
    if (depth >= 1.0){
        outColor = vec4(color, 1);
        return;
    }

    vec2 ndcPos = uv * 2.0 - 1.0;
    ndcPos.y = -ndcPos.y;
    vec4 worldPos = scene.cameraInvViewProj * vec4(ndcPos, depth, 1);
    worldPos /= worldPos.w;

    outColor = worldPos;
}
