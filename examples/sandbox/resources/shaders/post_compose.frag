#version 460 core

in  vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform float u_bloom_intensity;
uniform float u_exposure;

// ACES Filmic tone mapping curve (Krzysztof Narkowicz, 2015) — close enough to film-look
// and cheap. Operates in linear HDR; output is gamma 1.0 (we sRGB-encode below).
vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdr   = texture(u_scene, v_uv).rgb;
    vec3 bloom = texture(u_bloom, v_uv).rgb;

    vec3 color = hdr + bloom * u_bloom_intensity;
    color *= u_exposure;
    color = ACESFilm(color);

    // sRGB encode for display.
    color = pow(color, vec3(1.0 / 2.2));
    frag_color = vec4(color, 1.0);
}
