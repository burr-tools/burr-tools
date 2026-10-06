#version 440

// Overlay lines (axes, grid boundary, slab edges) as screen-space quads:
// Direct3D, Metal and Vulkan have no wide lines, and the spec asks for
// 1 to 2.5 dp lines, some dashed. Each segment is six vertices; `corner`
// says which end (x: 0 or 1) and which side (y: -1 or +1) a vertex is.

layout(location = 0) in vec3 p0;
layout(location = 1) in vec3 p1;
layout(location = 2) in vec2 corner;
layout(location = 3) in vec4 color;
layout(location = 4) in vec4 style;     // x: width px, y: dash px, z: gap px

layout(location = 0) out vec4 vColor;
layout(location = 1) out float vDist;
layout(location = 2) out vec2 vDash;
layout(location = 3) out float vAcross;   // pixels from the line's centre, signed
layout(location = 4) out float vHalf;     // half the line's width in pixels

layout(std140, binding = 0) uniform Buf {
    mat4 mvp;
    vec4 viewport;   // xy: target size in pixels, z: 1 when drawing in linear light
} ubuf;

void main()
{
    vec4 a = ubuf.mvp * vec4(p0, 1.0);
    vec4 b = ubuf.mvp * vec4(p1, 1.0);
    vec2 half_ = 0.5 * ubuf.viewport.xy;
    vec2 sa = a.xy / a.w * half_;
    vec2 sb = b.xy / b.w * half_;
    vec2 d = sb - sa;
    float len = length(d);
    vec2 dir = len > 1e-4 ? d / len : vec2(1.0, 0.0);
    vec2 n = vec2(-dir.y, dir.x);

    // one pixel wider on each side than the line, for the shader to fade
    // its edges out smoothly (multisampling alone leaves thin lines stepped)
    float halfWidth = style.x * 0.5;
    float reach = halfWidth + 1.0;
    vec4 p = corner.x < 0.5 ? a : b;
    p.xy += n * corner.y * reach / half_ * p.w;
    // a hair toward the viewer: the grid box and slab edges lie exactly on
    // voxel faces (the flat style has no bevels), and coincident depths
    // would let the face win or lose pixel by pixel
    p.z -= 5e-4 * p.w;
    gl_Position = p;

    vColor = color;
    vDist = corner.x * len;
    vDash = style.yz;
    vAcross = corner.y * reach;
    vHalf = halfWidth;
}
