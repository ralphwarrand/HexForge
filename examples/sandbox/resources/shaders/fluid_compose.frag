#version 460 core

// Modern high-fidelity screen-space fluid compose pass.
// Updates:
//   - Chromatic Aberration in refraction
//   - Procedural animated surface ripples (micro-normals)
//   - High-quality Fresnel + specular polish
//   - Beer-Lambert absorption with depth-aware scattering

in  vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_fluid_depth;
uniform sampler2D u_fluid_thickness;
uniform sampler2D u_scene_color;
uniform sampler2D u_scene_depth;

uniform mat4  u_proj;
uniform mat4  u_inv_proj;
uniform mat4  u_inv_view;
uniform vec3  u_view_light;   
uniform vec3  u_world_light;  
uniform vec3  u_light_color;
uniform float u_thickness_scale;
uniform float u_fresnel_f0;
uniform float u_refract_strength;
uniform float u_caustic_strength;
uniform int   u_enable_ssr;
uniform int   u_ssr_steps;
uniform float u_ssr_max_distance;
uniform vec3  u_water_tint;
uniform vec3  u_water_scatter;
uniform vec2  u_screen_size;
uniform float u_time; // Need to add time for animation

// Smooth procedural noise for ripples
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

vec3 getSurfaceRipples(vec3 worldPos, float distToCam) {
    // Fade out ripples with distance to prevent aliasing/dithering
    float fade = clamp(1.0 - distToCam * 0.05, 0.0, 1.0);
    if (fade <= 0.0) return vec3(0.0, 1.0, 0.0);
    
    vec2 p = worldPos.xz * 2.5;
    float t = u_time * 0.4;
    
    float n1 = noise(p + vec2(t * 0.15, t * 0.08));
    float n2 = noise(p * 1.8 - vec2(t * 0.1, t * 0.15));
    
    float eps = 0.1;
    float nx = (noise(p + vec2(eps, 0.0) + vec2(t * 0.15, t * 0.08)) - n1) / eps;
    float ny = (noise(p + vec2(0.0, eps) + vec2(t * 0.15, t * 0.08)) - n1) / eps;
    
    return normalize(vec3(-nx * fade * 0.3, 1.0, -ny * fade * 0.3));
}

vec3 reconstructViewFromVZ(vec2 uv, float view_z)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    vec4 v    = u_inv_proj * clip;
    v.xyz    /= v.w;
    vec3 ray  = normalize(v.xyz);
    return ray * (view_z / ray.z);
}

float linearizeSceneZ(float ndc_z)
{
    vec4 clip = vec4(0.0, 0.0, ndc_z * 2.0 - 1.0, 1.0);
    vec4 v    = u_inv_proj * clip;
    return v.z / v.w;
}

vec3 getWorldPos(vec2 uv, float ndc_z)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, ndc_z * 2.0 - 1.0, 1.0);
    vec4 world = u_inv_view * u_inv_proj * clip;
    return world.xyz / world.w;
}

vec3 skyProbe(vec3 dir_world)
{
    vec3 sun = normalize(u_world_light);
    float t  = clamp(dir_world.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky_top  = vec3(0.1, 0.25, 0.6);
    vec3 sky_horz = vec3(0.4, 0.6, 0.85);
    vec3 sky_grd  = vec3(0.15, 0.15, 0.2);
    vec3 sky      = mix(sky_grd, sky_horz, smoothstep(0.0, 0.4, t));
    sky           = mix(sky, sky_top, smoothstep(0.4, 1.0, t));
    float sun_d = max(dot(dir_world, sun), 0.0);
    sky += u_light_color * 3.0 * pow(sun_d, 1024.0); // Razor sharp
    sky += u_light_color * 0.1 * pow(sun_d, 64.0);
    return sky;
}

void main()
{
    float dc = texture(u_fluid_depth, v_uv).r;
    if (dc >= 0.0) discard;

    vec3 P = reconstructViewFromVZ(v_uv, dc);
    vec3 worldPos = (u_inv_view * vec4(P, 1.0)).xyz;
    float distToCam = length(P);
    
    vec2 ts = u_screen_size;
    vec2 texel = 1.0 / ts;

    const float NS = 1.5; // Tighter stride for more uniform smoothing
    vec2 sxp = v_uv + vec2( NS * texel.x, 0.0);
    vec2 sxm = v_uv + vec2(-NS * texel.x, 0.0);
    vec2 syp = v_uv + vec2(0.0,  NS * texel.y);
    vec2 sym = v_uv + vec2(0.0, -NS * texel.y);

    float dxp = texture(u_fluid_depth, sxp).r;
    float dxm = texture(u_fluid_depth, sxm).r;
    float dyp = texture(u_fluid_depth, syp).r;
    float dym = texture(u_fluid_depth, sym).r;

    vec3 ddx, ddy;
    if (dxp < 0.0 && dxm < 0.0) ddx = 0.5 * (reconstructViewFromVZ(sxp, dxp) - reconstructViewFromVZ(sxm, dxm));
    else if (dxp < 0.0) ddx = reconstructViewFromVZ(sxp, dxp) - P;
    else if (dxm < 0.0) ddx = P - reconstructViewFromVZ(sxm, dxm);
    else ddx = vec3(texel.x * NS, 0.0, 0.0);

    if (dyp < 0.0 && dym < 0.0) ddy = 0.5 * (reconstructViewFromVZ(syp, dyp) - reconstructViewFromVZ(sym, dym));
    else if (dyp < 0.0) ddy = reconstructViewFromVZ(syp, dyp) - P;
    else if (dym < 0.0) ddy = P - reconstructViewFromVZ(sym, dym);
    else ddy = vec3(0.0, texel.y * NS, 0.0);

    vec3 baseN = normalize(cross(ddx, ddy));
    if (baseN.z < 0.0) baseN = -baseN;

    // Mix in procedural ripples with distance-based intensity
    // Softened intensity (0.04 instead of 0.06) to prevent dithering
    vec3 rippleN_world = getSurfaceRipples(worldPos, distToCam);
    vec3 rippleN_view = normalize((inverse(u_inv_view) * vec4(rippleN_world, 0.0)).xyz);
    
    vec3 N = normalize(mix(baseN, rippleN_view, 0.04));

    vec3 V = normalize(-P);
    vec3 L = normalize(u_view_light);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float thickness = max(texture(u_fluid_thickness, v_uv).r, 0.0) * u_thickness_scale;

    // ---- Chromatic Refraction ----
    float eta = 1.0 / 1.33;
    vec3 R_refr_v = refract(-V, N, eta);
    if (length(R_refr_v) < 1e-4) R_refr_v = -V;

    float ref_scale = u_refract_strength * (1.0 - exp(-thickness * 0.4)) * 0.05;
    vec2 ref_dir = R_refr_v.xy;
    
    vec3 absorbed;
    {
        float chrom = 0.01;
        vec2 uvR = clamp(v_uv + ref_dir * ref_scale * (1.0 + chrom), 0.0, 1.0);
        vec2 uvG = clamp(v_uv + ref_dir * ref_scale, 0.0, 1.0);
        vec2 uvB = clamp(v_uv + ref_dir * ref_scale * (1.0 - chrom), 0.0, 1.0);
        
        float r = texture(u_scene_color, uvR).r;
        float g = texture(u_scene_color, uvG).g;
        float b = texture(u_scene_color, uvB).b;
        vec3 below = vec3(r, g, b);
        
        // SOTA Beer's Law: exp(-thickness * absorption_coeff)
        // Blue absorbs less, Red/Green absorb more.
        vec3 absorption = (vec3(1.0) - u_water_tint.rgb) * 1.5; 
        absorbed = below * exp(-thickness * absorption);
    }
    
    // ---- SSR ----
    vec3 reflect_color = vec3(0.0);
    float ssr_hit = 0.0;
    vec3 R_view = reflect(-V, N);
    vec3 R_world = normalize((u_inv_view * vec4(R_view, 0.0)).xyz);
    vec3 sky_col = skyProbe(R_world);

    if (u_enable_ssr != 0 && R_view.z < 0.0) {
        int steps_eff = clamp(u_ssr_steps, 8, 32);
        float step_size = (length(P) * u_ssr_max_distance) / float(steps_eff);
        vec3 ray_p = P + N * 0.02;
        for (int i = 0; i < steps_eff; ++i) {
            ray_p += R_view * step_size;
            vec4 clip = u_proj * vec4(ray_p, 1.0);
            vec3 ndc = clip.xyz / clip.w;
            if (any(greaterThan(abs(ndc.xy), vec2(1.0))) || ndc.z > 1.0 || ndc.z < -1.0) break;
            vec2 hit_uv = ndc.xy * 0.5 + 0.5;
            float scene_ndc_z = texture(u_scene_depth, hit_uv).r;
            float ray_ndc_z = ndc.z * 0.5 + 0.5;
            if (scene_ndc_z < 1.0 && ray_ndc_z > scene_ndc_z && ray_ndc_z < scene_ndc_z + 0.008) {
                reflect_color = texture(u_scene_color, hit_uv).rgb;
                ssr_hit = clamp(1.0 - float(i)/float(steps_eff), 0.0, 1.0);
                break;
            }
        }
    }
    vec3 reflection = mix(sky_col, reflect_color, ssr_hit);

    // ---- Fresnel & Specular ----
    float fresnel = u_fresnel_f0 + (1.0 - u_fresnel_f0) * pow(1.0 - NdotV, 5.0);
    vec3 H = normalize(L + V);
    // Extreme power (512) for razor-sharp highlights that don't look like bubbles.
    float spec = pow(max(dot(N, H), 0.0), 512.0) * 0.8; 

    // ---- Scattering ----
    float scatter_amount = max(dot(L, -V), 0.0) * (1.0 - exp(-thickness * 1.0));
    vec3 sss = u_water_scatter * scatter_amount * 0.2;

    // ---- Final Blend ----
    vec3 color = mix(absorbed + sss, reflection, fresnel);
    color += u_light_color * spec;

    float surface_alpha = smoothstep(0.02, 0.18, thickness);
    
    vec4 clip = u_proj * vec4(P, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;

    frag_color = vec4(color, surface_alpha);
}
