#version 440

layout(location = 0) in vec4 vColor;
layout(location = 1) in float vDist;
layout(location = 2) in vec2 vDash;
layout(location = 3) in float vAcross;
layout(location = 4) in float vHalf;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Buf {
    mat4 mvp;
    vec4 viewport;   // xy: target size in pixels, z: 1 when drawing in linear light
} ubuf;

vec3 toLinear(vec3 c)
{
    c = clamp(c, 0.0, 1.0);
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

void main()
{
    // coverage across the line: full inside, fading over the outer pixel
    float cover = clamp(vHalf + 0.5 - abs(vAcross), 0.0, 1.0);
    // and along it, at both ends of every dash
    if (vDash.x > 0.0) {
        float d = mod(vDist, vDash.x + vDash.y);
        cover *= clamp(min(d + 0.5, vDash.x - d + 0.5), 0.0, 1.0);
    }
    if (cover <= 0.0)
        discard;
    float a = vColor.a * cover;
    vec3 c = ubuf.viewport.z > 0.5 ? toLinear(vColor.rgb) : vColor.rgb;
    fragColor = vec4(c * a, a);
}
