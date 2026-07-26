#version 460 core

in  vec3 v_world_pos;
in  vec3 v_world_normal;
out vec4 frag_color;

uniform vec3  u_light_dir;   // direction *toward* the scene (we negate for L)
uniform vec3  u_view_pos;
uniform vec4  u_color;       // base colour

// Two-sided wrap-Lambert + Fresnel rim. Cloth is thin and should show wrap-around lighting
// rather than hard terminator; the back side gets dimmer but never goes completely dark.
void main()
{
    vec3 N = normalize(v_world_normal);
    vec3 V = normalize(u_view_pos - v_world_pos);
    if (dot(N, V) < 0.0) N = -N;                 // face the camera for shading
    vec3 L = normalize(-u_light_dir);

    // Wrap-Lambert: a small w pushes the terminator past the equator for soft shading.
    float w = 0.35;
    float NdotL = (max(dot(N, L), -w) + w) / (1.0 + w);

    // Schlick Fresnel rim — gives the silhouette of a fabric edge.
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

    vec3 base = u_color.rgb;
    vec3 ambient = base * 0.18;
    vec3 diffuse = base * NdotL * 0.85;
    vec3 rim     = mix(base, vec3(1.0), 0.6) * fresnel * 0.35;

    frag_color = vec4(ambient + diffuse + rim, 1.0);
}
