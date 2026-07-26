#version 460 core

in  vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_scene;
uniform float u_threshold;

void main()
{
    vec3 c = texture(u_scene, v_uv).rgb;
    // Soft knee around the threshold: smoothstep from threshold..threshold+0.5.
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float k = smoothstep(u_threshold, u_threshold + 0.5, luma);
    frag_color = vec4(c * k, 1.0);
}
