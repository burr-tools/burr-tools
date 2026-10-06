#version 440

// The scene was drawn and its multisampling resolved in linear light (so
// edges and translucent voxels blend evenly); here it is encoded to sRGB
// for the screen or the exported picture. Both are drawn with the same
// graphics API, so the fragment's position is the scene's texel.

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Buf {
    vec4 target;     // xy: size in pixels, z: 1 when the scene is in linear light
} ubuf;

layout(binding = 1) uniform sampler2D scene;

vec3 toSrgb(vec3 c)
{
    c = clamp(c, 0.0, 1.0);
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}

void main()
{
    vec4 c = texture(scene, gl_FragCoord.xy / ubuf.target.xy);
    // premultiplied: encode the colour, not colour * alpha
    if (ubuf.target.z > 0.5 && c.a > 0.0)
        c.rgb = toSrgb(c.rgb / c.a) * c.a;
    fragColor = c;
}
