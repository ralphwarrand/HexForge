#version 460 core

layout(location = 0) in vec2  a_quad;       // unit quad in [-1, 1]
layout(location = 1) in vec3  a_world_pos;  // per-instance world position (from CUDA-mapped VBO)
layout(location = 2) in uint  a_material_id;
layout(location = 3) in uint  a_flags;      // bit 0 = rigid body member

layout(std140, binding = 2) uniform Materials {
    vec4 u_colors[32];
};

uniform mat4 u_view;
uniform mat4 u_proj;
uniform float u_radius;
uniform int   u_hide_rigid;  // 1 = skip drawing particles flagged as rigid-body members
uniform int   u_hide_fluid;  // 1 = skip drawing fluid particles (they get SSFR instead)
uniform int   u_hide_cloth;  // 1 = skip drawing cloth particles (wireframe overlay replaces them)

out vec3 v_sphere_center_view;
out vec3 v_quad_pos_view;
out vec4 v_color;
flat out float v_radius;
flat out int   v_culled;

void main()
{
    // Cull two kinds of particles depending on what other pipelines are drawing them:
    //   - rigid-body members in mesh mode (rendered as the body's source mesh)
    //   - fluid particles in mesh mode    (rendered via screen-space fluid surface)
    bool cull = false;
    if ((u_hide_rigid != 0) && ((a_flags & 1u) != 0u)) cull = true;
    if ((u_hide_fluid != 0) && ((a_flags & 2u) != 0u)) cull = true;
    if ((u_hide_cloth != 0) && ((a_flags & 4u) != 0u)) cull = true;
    v_culled = cull ? 1 : 0;
    if (cull) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside clip space
        v_sphere_center_view = vec3(0.0);
        v_quad_pos_view = vec3(0.0);
        v_color = vec4(0.0);
        v_radius = 0.0;
        return;
    }

    vec4 center_view = u_view * vec4(a_world_pos, 1.0);
    vec3 offset_view = vec3(a_quad * u_radius, 0.0);
    vec4 pos_view = center_view + vec4(offset_view, 0.0);

    v_sphere_center_view = center_view.xyz;
    v_quad_pos_view = pos_view.xyz;
    v_color = u_colors[a_material_id];
    v_radius = u_radius;

    gl_Position = u_proj * pos_view;
}
