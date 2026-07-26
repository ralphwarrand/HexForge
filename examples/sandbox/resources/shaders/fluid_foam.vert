#version 460 core

// Foam / spray billboard pass. Renders ONLY particles that the CUDA foam classifier marked
// as splash (bit 3 of flags). Each foam particle is a small soft-disc additive sprite.

layout(location = 0) in vec2  a_quad;
layout(location = 1) in vec3  a_world_pos;
layout(location = 2) in uint  a_flags;

uniform mat4  u_view;
uniform mat4  u_proj;
uniform float u_radius;

out vec2 v_uv;
flat out int v_culled;

void main()
{
    // Only fluid particles classified as foam (bit 1 set AND bit 3 set).
    if ((a_flags & 8u) == 0u || (a_flags & 2u) == 0u) {
        v_culled = 1;
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        v_uv = vec2(0.0);
        return;
    }
    v_culled = 0;
    vec4 view_center = u_view * vec4(a_world_pos, 1.0);
    vec3 offset      = vec3(a_quad * u_radius, 0.0);
    gl_Position = u_proj * (view_center + vec4(offset, 0.0));
    v_uv = a_quad;
}
