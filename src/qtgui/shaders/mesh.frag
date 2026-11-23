#version 440

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in float vAlpha;
layout(location = 3) in vec4 vEdge;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Buf {
    mat4 mvp;
    mat4 modelView;
    vec4 light;
    vec4 dim;
    vec4 outline;    // x: width in pixels, y: strength, z: dash length in pixels, w: depth bias
    vec4 misc;       // x: 1 when drawing in linear light
} ubuf;

vec3 toLinear(vec3 c)
{
    c = clamp(c, 0.0, 1.0);
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

// the mock's shade(): factors above 1 lighten toward white, below darken
vec3 shade(vec3 c, float f)
{
    return f > 1.0 ? c + (vec3(1.0) - c) * (f - 1.0) : c * f;
}

/* How much of the face outline covers this fragment (C06, the mock: the
 * shape's edges stroked in black at 32 %; the boundaries between voxels
 * across a flat face in a line half as wide at a third of the strength).
 * vEdge.xyz are the distances to the triangle's three sides as barycentric
 * coordinates; a side that is no part of the outline carries +2, so it is
 * never near, and a side drawn thin carries +4. Dividing by the screen-space
 * rate of change turns them into pixels, which keeps the line the same
 * width at every zoom. */
float outlineCoverage()
{
    vec3 b = vEdge.xyz;
    vec3 w = max(fwidth(b), vec3(1e-6));    // before any branch: derivatives need uniform flow
    vec3 thin = step(vec3(3.5), b);         // +4: a thin line
    vec3 none = step(vec3(1.5), b) * (1.0 - thin);   // +2: no line
    vec3 t = b - thin * 4.0 - none * 2.0;   // the true coordinates, for the position along a side
    vec3 px = (b - thin * 4.0) / w;         // a thin side is measured; one without a line stays far

    float d = 1e6;
    float along = 0.0;
    float isThin = 0.0;
    if (px.x < d) { d = px.x; along = t.y / w.y; isThin = thin.x; }    // the side opposite corner 0 runs from 1 to 2
    if (px.y < d) { d = px.y; along = t.z / w.z; isThin = thin.y; }
    if (px.z < d) { d = px.z; along = t.x / w.x; isThin = thin.z; }

    float width = ubuf.outline.x * mix(1.0, 0.5, isThin);
    float cover = 1.0 - smoothstep(width - 0.5, width + 0.5, d);
    if (vEdge.w > 0.5 && mod(along, 2.0 * ubuf.outline.z) > ubuf.outline.z)
        cover = 0.0;
    return cover * ubuf.outline.y * mix(1.0, 0.35, isThin);
}

void main()
{
    float f = 0.92;                     // unlit: flat, slightly darkened (C06 / Settings Lighting)
    if (ubuf.light.w > 0.5) {
        vec3 n = normalize(vNormal);
        f = 0.62 + 0.5 * max(0.0, dot(n, normalize(ubuf.light.xyz)));
    }
    // Shaded in sRGB, as the mock and legacy shade, so a face has exactly
    // their colour; then into linear light, where it is blended.
    vec3 c = shade(vColor.rgb, f);
    if (ubuf.misc.x > 0.5)
        c = toLinear(c);
    // the outline is stroked over the face at its own strength, as the
    // mock strokes after filling: a translucent (variable) voxel keeps a
    // clear dashed edge (premultiplied: black at `cover` over the face)
    float a = vColor.a * vAlpha;
    float cover = outlineCoverage() * vAlpha;
    fragColor = vec4(c * a * (1.0 - cover), cover + a * (1.0 - cover));
}
