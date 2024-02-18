#version 460
#include "standard.glsl"

layout(location = 0) out vec4 outColor;

float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

// fragCoord.xy(ピクセル単位)と同じ空間に揃える
vec2 worldToWindowSpace(vec3 pos){
    vec4 clipPos = scene.cameraViewProj * vec4(pos, 1);
    vec2 ndcPos = clipPos.xy / clipPos.w;
    ndcPos.y = -ndcPos.y;
    return (ndcPos * 0.5 + 0.5) * scene.screenResolution;
}

float colormap_red(float x) {
    if (x < 0.75) {
        return 1012.0 * x - 389.0;
    } else {
        return -1.11322769567548E+03 * x + 1.24461193212872E+03;
    }
}

float colormap_green(float x) {
    if (x < 0.5) {
        return 1012.0 * x - 129.0;
    } else {
        return -1012.0 * x + 899.0;
    }
}

float colormap_blue(float x) {
    if (x < 0.25) {
        return 1012.0 * x + 131.0;
    } else {
        return -1012.0 * x + 643.0;
    }
}

vec4 colormap(float x) {
    float r = clamp(colormap_red(x) / 255.0, 0.0, 1.0);
    float g = clamp(colormap_green(x) / 255.0, 0.0, 1.0);
    float b = clamp(colormap_blue(x) / 255.0, 0.0, 1.0);
    return vec4(r, g, b, 1.0);
}

void main(){
    vec2 resolution = scene.screenResolution;
    vec2 inverseVP = 1.0 / resolution;
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord * inverseVP;

//#define STEP_COUNT 1
//#define THICKNESS 2
//#define VISUALIZE THICKNESS
#define RAYMARCH_DDA

    vec3 color = texture(baseColorImage, uv).xyz;
    float depth = texture(depthImage, uv).x;

#ifdef VISUALIZE
    outColor = vec4(0);
#else
    outColor = vec4(color, 1);
#endif

    if (depth >= 1.0){
        return;
    }

    // NOTE: BRDFが十分に小さい箇所はスキップして高速化
    vec3 specularBrdf = texture(specularBrdfImage, uv).xyz;
    //if(all(lessThan(specularBrdf, vec3(0.1)))){
    //    return;
    //}

    vec2 ndcPos = uv * 2.0 - 1.0;
    ndcPos.y = -ndcPos.y;
    vec4 worldPos = scene.cameraInvViewProj * vec4(ndcPos, depth, 1);
    worldPos /= worldPos.w;

    vec3 V = normalize(scene.cameraPos.xyz - worldPos.xyz);
    vec3 N = normalize(texture(normalImage, uv).xyz);
    vec3 R = reflect(-V, N);

    const float maxRayDistance = 10.0;
    const float maxThickness = 0.00005;
    const int maxRaySteps = 50;
    const float randomness = 0.0;

#ifdef RAYMARCH_DDA
    // _ws: world space
    // _px: screen space (pixel)
    vec3 start_ws = worldPos.xyz;
    vec3 end_ws = worldPos.xyz + R * maxRayDistance;
    vec2 start_px = worldToWindowSpace(worldPos.xyz); // fragCoord.xy と同値 (小数の0.5をそのままに)
    vec2 end_px = worldToWindowSpace(end_ws.xyz);

    // 始点と終点のデプスを計算しておく
    // ここで NDC 使うので worldToWindowSpace に渡す
    vec4 _start_ndc = scene.cameraViewProj * vec4(start_ws, 1);
    vec4 _end_ndc = scene.cameraViewProj * vec4(end_ws, 1);
    float startDepth = _start_ndc.z / _start_ndc.w;
    float endDepth = _end_ndc.z / _end_ndc.w;

    // カメラの後ろ側にいった場合は終了
    if(_end_ndc.w <= 0.0){
        return;
    }

    int dx = int(end_px.x) - int(start_px.x);
    int dy = int(end_px.y) - int(start_px.y);

    int stepSize = 32;
    //stepSize = int(1.0 + stepSize * (1.0 - min(1.0, -startDepth / 100.0)));
    int steps = int(max(abs(dx), abs(dy)) / float(stepSize));

    float x_inc = dx / float(steps);
    float y_inc = dy / float(steps);

    // 次のピクセルから始めるので inc を足す
    float randomOffset = random(uv) * randomness;
    float x = start_px.x + (x_inc * (1.0 + randomOffset));
    float y = start_px.y + (y_inc * (1.0 + randomOffset));

    for(int i = 0; i < steps; i++){
        vec2 uv = vec2(x, y) / resolution;
        float depth = texture(depthImage, uv).x;
        if (depth >= 1.0){
            return;
        }

        // レイデプスは始点と終点のデプスの線形補間でいい
        float rayDepth = mix(startDepth, endDepth, float(i) / steps);

        float thickness = rayDepth - depth;
        if (0.0 < thickness && thickness < maxThickness){
#ifdef VISUALIZE
    #if VISUALIZE == STEP_COUNT
            outColor = vec4(0, float(i) / maxRaySteps, 0, 1);
    #else
            outColor = vec4(thickness / maxThickness, 0, 0, 1);
    #endif
#else
            vec3 color = texture(baseColorImage, uv).xyz;
            outColor += vec4(color * specularBrdf * scene.ssrIntensity, 1);
#endif
            return;
        }
        if(thickness >= maxThickness){
            return;
        }
        
        x += x_inc;
        y += y_inc;
    }
#ifdef VISUALIZE
    #if VISUALIZE == STEP_COUNT
        outColor = vec4(0, steps / float(maxRaySteps), 0, 1);
    #else
        outColor = vec4(0, 0, 0, 1);
    #endif
#endif

#else // RAYMARCH_DDA
    const float stepSize = maxRayDistance / maxRaySteps;
    for (int i = 1; i <= maxRaySteps; i++){
        // Ray march
        // NOTE: マイナスオフセットは逆方向にめり込む可能性があるので使わない
        // NOTE: オフセットは各ステップでランダムに変化させると綺麗になる
        // NOTE: オフセットは 1.0 よりも少し大きめにとるとさらに綺麗になる
        float randomOffset = random(uv + i) * randomness;
        vec3 rayPos = worldPos.xyz + R * stepSize * (i + randomOffset);
        vec4 vpPos = (scene.cameraViewProj * vec4(rayPos, 1));
        float rayDepth = vpPos.z / vpPos.w;

        // Get depth from depth buffer
        vec2 screenPos = vpPos.xy / vpPos.w;
        screenPos.y = -screenPos.y;
        vec2 uv = (screenPos * 0.5 + 0.5);
        float depth = texture(depthImage, uv).x;
        if (depth >= 1.0){
            return;
        }

        // Get color from base color buffer
        float thickness = rayDepth - depth;
        if (0.0 < thickness && thickness < maxThickness){
#ifdef VISUALIZE
    #if VISUALIZE == STEP_COUNT
            outColor = vec4(0, float(i) / maxRaySteps, 0, 1);
    #else
            outColor = vec4(thickness / maxThickness, 0, 0, 1);
    #endif
#else
            vec3 color = texture(baseColorImage, uv).xyz;
            outColor += vec4(color * specularBrdf * scene.ssrIntensity, 1);
#endif
            return;
        }
        if(thickness >= maxThickness){
            return;
        }
    }
#ifdef VISUALIZE
    #if VISUALIZE == STEP_COUNT
        outColor = vec4(0, 1, 0, 1);
    #else
        outColor = vec4(0, 0, 0, 1);
    #endif
#endif
#endif // RAYMARCH_DDA
}
