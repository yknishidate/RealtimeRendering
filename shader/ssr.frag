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
    outColor = vec4(color, 1);
    if (depth >= 1.0){
        return;
    }

    vec2 ndcPos = uv * 2.0 - 1.0;
    ndcPos.y = -ndcPos.y;
    vec4 worldPos = scene.cameraInvViewProj * vec4(ndcPos, depth, 1);
    worldPos /= worldPos.w;

    vec3 V = normalize(scene.cameraPos.xyz - worldPos.xyz);
    vec3 N = texture(normalImage, uv).xyz;
    vec3 R = reflect(-V, N);

    const int maxRaySteps = 100;
    const float stepSize = 0.1;
    const float reflectivity = 0.5;
    for (int i = 1; i <= maxRaySteps; i++){
        // Ray march
        vec3 rayPos = worldPos.xyz + R * stepSize * float(i);
        vec4 vpPos = (scene.cameraViewProj * vec4(rayPos, 1));
        float rayDepth = vpPos.z / vpPos.w;

        // Get depth from depth buffer
        vec2 screenPos = vpPos.xy / vpPos.w;
        screenPos.y = -screenPos.y;
        vec2 uv = (screenPos * 0.5 + 0.5);
        float depth = texture(depthImage, uv).x;
        if (depth >= 1.0){
            break;
        }

        // Get color from base color buffer
        if (depth < rayDepth){
            vec3 color = texture(baseColorImage, uv).xyz;
            outColor += vec4(color, 1) * reflectivity;
            return;
        }
    }
}
