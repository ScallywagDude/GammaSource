#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outColor;
layout(set=0, binding=0) uniform sampler2D SceneColor;
layout(set=0, binding=1) uniform TonemapConstants {
    vec4 controls;
} parameters;
vec3 acesFilmic(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
vec3 encodeSRGB(vec3 x)
{
    x = clamp(x, 0.0, 1.0);
    return mix(12.92 * x, 1.055 * pow(x, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), x));
}
void main()
{
    vec4 scene = texture(SceneColor, uv);
    vec3 linearColor = max(scene.rgb, vec3(0.0)) * max(parameters.controls.x, 0.0);
    vec3 mapped = parameters.controls.y > 0.5 ? acesFilmic(linearColor) : clamp(linearColor, 0.0, 1.0);
    outColor = vec4(parameters.controls.z > 0.5 ? encodeSRGB(mapped) : mapped, scene.a);
}
