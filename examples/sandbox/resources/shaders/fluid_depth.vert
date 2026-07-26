#version 460 core

// Renders fluid particle sphere impostors into a depth + thickness FBO.
// Only emits geometry for particles whose flags bit 1 is set (fluid phase).

layout(location = 0) in vec2  a_quad;
layout(location = 1) in vec3  a_world_pos;
layout(location = 2) in uint  a_flags;

uniform mat4 u_view;
uniform mat4 u_proj;
uniform float u_radius;

out vec3 v_view_pos;
flat out vec3 v_view_center;
flat out int  v_culled;

void main()
{
    if ((a_flags & 2u) == 0u) {
        v_culled = 1;
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        v_view_pos = vec3(0.0);
        v_view_center = vec3(0.0);
        return;
    }
    v_culled = 0;
    
    // Adaptive Radius: Isolated particles are solid droplets (0.8x); 
    // Crowded particles fuse into a smooth pool (1.8x).
    uint neighbours = (a_flags >> 24) & 0xFFu;
    float density_factor = clamp(float(neighbours) / 10.0, 0.0, 1.0);
    float adaptive_scale = mix(0.8, 1.8, density_factor);
    float eff_radius = u_radius * adaptive_scale;

    vec4 view_center = u_view * vec4(a_world_pos, 1.0);
    vec3 offset      = vec3(a_quad * eff_radius, 0.0);
    vec4 view_pos    = view_center + vec4(offset, 0.0);
    v_view_center = view_center.xyz;
    v_view_pos    = view_pos.xyz;
    gl_Position   = u_proj * view_pos;
}
