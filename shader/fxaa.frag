#version 460
#include "standard.glsl"

layout(location = 0) out vec4 outColor;

#ifndef FXAA_REDUCE_MIN
    #define FXAA_REDUCE_MIN   (1.0/ 128.0)
#endif
#ifndef FXAA_REDUCE_MUL
    #define FXAA_REDUCE_MUL   (1.0 / 8.0)
#endif
#ifndef FXAA_SPAN_MAX
    #define FXAA_SPAN_MAX     8.0
#endif

vec4 fxaa(sampler2D tex, vec2 fragCoord, vec2 resolution,
          vec2 v_rgbNW, vec2 v_rgbNE, 
          vec2 v_rgbSW, vec2 v_rgbSE, 
          vec2 v_rgbM) {
    vec4 color;
    vec2 inverseVP = vec2(1.0 / resolution.x, 1.0 / resolution.y);
    vec3 rgbNW = texture(tex, v_rgbNW).xyz;
    vec3 rgbNE = texture(tex, v_rgbNE).xyz;
    vec3 rgbSW = texture(tex, v_rgbSW).xyz;
    vec3 rgbSE = texture(tex, v_rgbSE).xyz;
    vec4 texColor = texture(tex, v_rgbM);
    vec3 rgbM  = texColor.xyz;
    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
                          (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
              max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
              dir * rcpDirMin)) * inverseVP;
    
    vec3 rgbA = 0.5 * (
        texture(tex, fragCoord * inverseVP + dir * (1.0 / 3.0 - 0.5)).xyz +
        texture(tex, fragCoord * inverseVP + dir * (2.0 / 3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(tex, fragCoord * inverseVP + dir * -0.5).xyz +
        texture(tex, fragCoord * inverseVP + dir * 0.5).xyz);

    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax)){
        color = vec4(rgbA, texColor.a);
    } else {
        color = vec4(rgbB, texColor.a);
    }
    return color;
}

vec3 tonemap(vec3 color, float exposure){
    return 1.0 - exp(-color * exposure);
}

void main(){
    if(scene.enableFXAA == 1){
        vec2 resolution = scene.screenResolution;
        vec2 inverseVP = 1.0 / resolution;
        vec2 fragCoord = gl_FragCoord.xy;
        vec2 v_rgbNW = (fragCoord + vec2(-1.0, -1.0)) * inverseVP;
        vec2 v_rgbNE = (fragCoord + vec2(1.0, -1.0)) * inverseVP;
        vec2 v_rgbSW = (fragCoord + vec2(-1.0, 1.0)) * inverseVP;
        vec2 v_rgbSE = (fragCoord + vec2(1.0, 1.0)) * inverseVP;
        vec2 v_rgbM = vec2(fragCoord * inverseVP);

        vec3 color;
        if(scene.enableSSR == 1){
            color = fxaa(compositeColorImage, fragCoord, resolution,
                         v_rgbNW, v_rgbNE, v_rgbSW, v_rgbSE, v_rgbM).xyz;
        }else{
            color = fxaa(baseColorImage, fragCoord, resolution,
                         v_rgbNW, v_rgbNE, v_rgbSW, v_rgbSE, v_rgbM).xyz;
        }
        outColor = vec4(gammaCorrect(tonemap(color, scene.exposure), 1.0 / 2.2), 1.0);
    }else{
        vec3 color;
        if(scene.enableSSR == 1){
            color = texture(compositeColorImage, 
                            vec2(gl_FragCoord.xy) / scene.screenResolution).xyz;
        }else{
            color = texture(baseColorImage, 
                            vec2(gl_FragCoord.xy) / scene.screenResolution).xyz;
        }
        outColor = vec4(gammaCorrect(tonemap(color, scene.exposure), 1.0 / 2.2), 1.0);
        //vec4 averageColor = vec4(0.0);
        //for(int dx = -1; dx <= 1; dx++){
        //    for(int dy = -1; dy <= 1; dy++){
        //        averageColor += texture(baseColorImage, ((gl_FragCoord.xy + vec2(dx, dy)) / scene.screenResolution));
        //    }
        //}
        //outColor = averageColor / 9.0;
    }
}
