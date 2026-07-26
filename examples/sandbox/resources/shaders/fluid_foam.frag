#version 460 core

in  vec2 v_uv;
flat in int v_culled;
out vec4 frag_color;

uniform float u_alpha;

void main()
{
    if (v_culled != 0) discard;
    // Soft round sprite — alpha falls off from centre to edge.
    float r = length(v_uv);
    if (r > 1.0) discard;
    float a = u_alpha * (1.0 - r) * (1.0 - r);
    frag_color = vec4(vec3(0.95, 0.97, 1.0), a);
}
