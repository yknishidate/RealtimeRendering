#version 460
#include "standard.glsl"

layout(location = 0) out vec4 outColor;

float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

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

    const int maxRaySteps = 50;
    const float maxRayDistance = 10.0;
    const float stepSize = maxRayDistance / maxRaySteps;
    const float reflectivity = 0.5;
    const float maxThickness = 0.00005;
    for (int i = 1; i <= maxRaySteps; i++){
        // Ray march
        // NOTE: マイナスオフセットは逆方向にめり込む可能性があるので使わない
        // NOTE: オフセットは各ステップでランダムに変化させると綺麗になる
        // NOTE: オフセットは 1.0 よりも少し大きめにとるとさらに綺麗になる
        float randomOffset = random(uv + i) * 1.5;
        vec3 rayPos = worldPos.xyz + R * stepSize * (i + randomOffset);
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
        float thickness = rayDepth - depth;
        if (0.0 < thickness && thickness < maxThickness){
            vec3 color = texture(baseColorImage, uv).xyz;
            outColor += vec4(color, 1) * reflectivity;
            return;
        }
    }
}
