#version 460 core

// Separable box-ish blur for the additive thickness buffer. Each particle sphere splats a
// chord-length disc; raw thickness is full of bumpy ridges between particles. Smoothing the
// thickness map (independent of the depth bilateral) gives the Beer-Lambert absorption and
// the foam alpha a uniform reading.
//
// We use a small Gaussian (sigma_pix = 5) so the absorption profile follows the *actual*
// extent of the fluid mass rather than its per-splat fine structure. No range-stop term —
// thickness is allowed to bleed across silhouettes, that's actually correct for an integrated
// quantity (a thinner edge having "smoothed" thickness from the bulk is fine).

in  vec2 v_uv;
out float frag_thickness;

uniform sampler2D u_thickness;
uniform vec2  u_dir;
uniform vec2  u_texel;

void main()
{
    const float sigma_pix   = 5.0;
    const float inv_2sigma2 = 0.5 / (sigma_pix * sigma_pix);
    const int   kTaps       = 8;

    float sum   = texture(u_thickness, v_uv).r;
    float w_sum = 1.0;

    for (int i = 1; i <= kTaps; ++i) {
        float fi  = float(i);
        vec2  off = u_dir * u_texel * fi;
        float w   = exp(-fi * fi * inv_2sigma2);
        sum   += texture(u_thickness, v_uv + off).r * w;
        sum   += texture(u_thickness, v_uv - off).r * w;
        w_sum += 2.0 * w;
    }
    frag_thickness = sum / w_sum;
}
