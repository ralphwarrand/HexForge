#version 460 core
precision highp float;

in  vec2 v_uv;
out vec4 frag_color;

uniform mat4  u_inv_view_proj;
uniform vec3  u_view_pos;
uniform float u_grid_y;
uniform mat4  u_view_proj;

layout(std140, binding = 0) uniform RenderData {
    mat4 view;
    mat4 projection;
    vec3 view_pos;
    float _pad1;
    vec3 light_dir;
    float _pad2;
    vec3 light_color;
    float _pad3;
    int  wireframe;
};

uniform sampler2DShadow shadow_map;
uniform mat4 u_light_space_matrix; 

float ShadowCalculation(vec3 worldPos, vec3 N, vec3 L) {
    vec4 lightSpacePos = u_light_space_matrix * vec4(worldPos, 1.0);
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;
    float bias = max(0.005 * (1.0 - max(dot(N, L), 0.0)), 0.0005);
    float shadow = 0.0;
    ivec2 texSize = textureSize(shadow_map, 0);
    vec2 texelSize = 1.0 / vec2(texSize);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            shadow += texture(shadow_map, vec3(proj.xy + vec2(x, y) * texelSize, proj.z - bias));
        }
    }
    return shadow / 9.0;
}

float gridLine(float x, float spacing) {
    float d  = abs(fract(x / spacing - 0.5) - 0.5);
    float dd = fwidth(x / spacing);
    return 1.0 - smoothstep(0.0, dd, d);
}

void main() {
    vec4 ndc_near = vec4(v_uv * 2.0 - 1.0, -1.0, 1.0);
    vec4 ndc_far  = vec4(v_uv * 2.0 - 1.0,  1.0, 1.0);
    vec4 w_near = u_inv_view_proj * ndc_near; w_near /= w_near.w;
    vec4 w_far  = u_inv_view_proj * ndc_far;  w_far  /= w_far.w;
    vec3 ro = w_near.xyz;
    vec3 rd = normalize(w_far.xyz - w_near.xyz);

    if (abs(rd.y) < 1e-4) discard;
    float t = (u_grid_y - ro.y) / rd.y;
    if (t < 0.0) discard;
    vec3 hit = ro + rd * t;

    vec3 N = vec3(0.0, 1.0, 0.0);
    vec3 V = normalize(u_view_pos - hit);
    vec3 L = normalize(-light_dir);
    vec3 H = normalize(V + L);

    float minor = max(gridLine(hit.x, 1.0), gridLine(hit.z, 1.0));
    float sub   = max(gridLine(hit.x, 0.2), gridLine(hit.z, 0.2));
    float dist = length(hit.xz - u_view_pos.xz);
    float fade = exp(-dist * 0.02);

    vec3 base_color = vec3(0.08, 0.08, 0.1); 
    vec3 line_color = mix(vec3(0.15, 0.15, 0.2), vec3(0.4, 0.4, 0.5), minor);
    line_color = mix(line_color, vec3(0.2, 0.2, 0.25), sub * 0.5);
    
    float alpha = mix(0.1, 1.0, minor * 0.4 + sub * 0.1) * fade;
    vec3 color_on_ground = mix(base_color, line_color, alpha);
    
    float rough = 0.15;
    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float spec = pow(NdotH, 128.0) * 0.5; 

    float shadow = ShadowCalculation(hit, N, L);
    vec3 ambient = vec3(0.02) * color_on_ground;
    vec3 final_color = ambient + (color_on_ground * NdotL + spec * light_color) * shadow;

    float horizon = 1.0 - smoothstep(300.0, 800.0, dist);
    if (horizon < 0.001) discard;

    frag_color = vec4(final_color, horizon);

    vec4 clip = u_view_proj * vec4(hit, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;
}
