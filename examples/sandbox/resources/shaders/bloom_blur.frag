#version 460 core

// Separable 9-tap Gaussian blur. u_dir = (1,0) for horizontal, (0,1) for vertical.
// Hardware-bilinear taps cut the sample count in half by exploiting linear filtering at
// non-integer offsets (Sigg & Hadwiger 2005, "Fast Third-Order Texture Filtering").

in  vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_image;
uniform vec2 u_dir;
uniform vec2 u_texel;

void main()
{
    // Weights for a 9-tap Gaussian (sigma ~= 2.5).
    const float w0 = 0.227027;
    const float w1 = 0.316216;  // combined weight of taps at ±1.3846
    const float w2 = 0.070270;  // combined weight of taps at ±3.2308
    const float o1 = 1.3846153846;
    const float o2 = 3.2307692308;

    vec3 acc = texture(u_image, v_uv).rgb * w0;
    vec2 off1 = u_dir * u_texel * o1;
    vec2 off2 = u_dir * u_texel * o2;
    acc += texture(u_image, v_uv + off1).rgb * w1;
    acc += texture(u_image, v_uv - off1).rgb * w1;
    acc += texture(u_image, v_uv + off2).rgb * w2;
    acc += texture(u_image, v_uv - off2).rgb * w2;
    frag_color = vec4(acc, 1.0);
}
