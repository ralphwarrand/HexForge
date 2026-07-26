#version 460 core

in vec3 v_view_pos;
flat in vec3 v_view_center;
flat in int  v_culled;

uniform mat4  u_proj;
uniform float u_radius;

layout(location = 0) out float frag_depth;     // view-space z (negative)
layout(location = 1) out float frag_thickness; // additive accumulation per pixel

void main()
{
    if (v_culled != 0) discard;

    // Ray-sphere intersection in view space (camera at origin).
    vec3 d  = normalize(v_view_pos);
    vec3 oc = -v_view_center;
    float b = dot(d, oc);
    float h = b * b - dot(oc, oc) + u_radius * u_radius;
    if (h < 0.0) discard;

    float t   = -b - sqrt(h);
    vec3  hit = d * t;
    frag_depth     = hit.z;                    // negative
    frag_thickness = 2.0 * sqrt(h) * 0.1;      // chord length × scale; additive blend

    // Write GL depth so subsequent same-pass fluid particles depth-test correctly.
    vec4 clip = u_proj * vec4(hit, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;
}
