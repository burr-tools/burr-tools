#version 440

// Voxel geometry and translucent overlays (the layer slab) of the 3D view.

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec3 cell;      // grid coordinate of the voxel the face belongs to
layout(location = 4) in vec4 edge;      // outline: distances to the triangle's sides (+2: no outline there), w dashed

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out float vAlpha;
layout(location = 3) out vec4 vEdge;

layout(std140, binding = 0) uniform Buf {
    mat4 mvp;
    mat4 modelView;
    vec4 light;      // xyz: light direction in view space; w: 1 lit, 0 flat
    vec4 dim;        // x: layer axis (0, 1, 2; < 0 = no dimming), y: layer, z: alpha outside it, w: alpha of everything
    vec4 outline;    // x: width in pixels, y: strength, z: dash length in pixels, w: depth bias
    vec4 misc;       // x: 1 when drawing in linear light
} ubuf;

void main()
{
    vColor = color;
    vEdge = edge;
    vNormal = mat3(ubuf.modelView) * normal;

    float a = ubuf.dim.w;
    if (ubuf.dim.x >= 0.0) {
        float c = ubuf.dim.x < 0.5 ? cell.x : (ubuf.dim.x < 1.5 ? cell.y : cell.z);
        if (abs(c - ubuf.dim.y) > 0.5)
            a *= ubuf.dim.z;
    }
    vAlpha = a;

    gl_Position = ubuf.mvp * vec4(position, 1.0);
    // the layer slab's faces lie exactly on voxel faces in the flat style;
    // a hair toward the viewer, so it tints them rather than fighting them
    gl_Position.z -= ubuf.outline.w * gl_Position.w;
}
