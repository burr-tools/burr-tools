#version 440

// The last step of a 3D-view frame: one triangle that covers the target,
// copying the linear-light scene into it (composite.frag).

layout(location = 0) in vec2 position;

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
}
