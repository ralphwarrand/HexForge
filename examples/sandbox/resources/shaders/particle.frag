#version 460 core

in vec3 v_sphere_center_view;
in vec3 v_quad_pos_view;
in vec4 v_color;
flat in float v_radius;
flat in int   v_culled;

uniform mat4 u_proj;
uniform vec3 u_view_light_dir; // sun direction in view space (toward light)

out vec4 frag_color;

// View-space lighting. Camera is at origin looking down -z, so view direction from a fragment
// to the camera is simply -normalize(fragment_view). Everything stays in linear [0,1]-ish range
// so the post-processing pipeline can do its job — bright pixels are reserved for genuine
// highlights, not for "the floor".
void main()
{
    if (v_culled != 0) discard;

    // Ray-cast against the sphere impostor in view space.
    vec3 ray_o = vec3(0.0);
    vec3 ray_d = normalize(v_quad_pos_view);
    vec3 c     = v_sphere_center_view;
    float r    = v_radius;

    vec3 oc = ray_o - c;
    float b = dot(ray_d, oc);
    float h = b * b - dot(oc, oc) + r * r;
    if (h < 0.0) discard;
    float t = -b - sqrt(h);
    vec3 hit_view = ray_o + t * ray_d;

    vec3 N = normalize(hit_view - c);
    vec3 V = normalize(-hit_view);
    vec3 L = normalize(u_view_light_dir);
    vec3 H = normalize(L + V);

    vec3 base = v_color.rgb;

    // Diffuse + ambient.
    float NdotL = max(dot(N, L), 0.0);
    vec3 ambient = base * 0.18;
    vec3 diffuse = base * NdotL * 0.85;

    // Blinn-Phong specular.
    float NdotH = max(dot(N, H), 0.0);
    float spec  = pow(NdotH, 48.0) * 0.45;

    // Schlick Fresnel — gives a cool rim that scales with grazing angle.
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0);
    vec3 rim = mix(base, vec3(1.0), 0.6) * fresnel * 0.35;

    vec3 lit = ambient + diffuse + vec3(spec) + rim;
    frag_color = vec4(lit, 1.0);

    // Write correct depth so post & other geometry blend properly.
    vec4 clip = u_proj * vec4(hit_view, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;
}
