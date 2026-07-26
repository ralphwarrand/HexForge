#version 460 core

// Bilateral smoothing of view-space fluid depth (van der Laan 2009 + Truong et al. 2014).
//
// The trick: pair a moderate sigma with a TAP STRIDE so the per-iter kernel is wide without
// being expensive (each iter still takes 2*kTaps+1 samples but spans `kStride * kTaps`
// texels). Multiple separable iterations stack via sqrt(N), giving an effective screen-space
// radius easily large enough to swallow the per-sphere caps at any sensible camera distance.
//
// Range term (exp(-dz²/2σ²)) edge-stops against silhouettes and the background sentinel.

in  vec2 v_uv;
out float frag_depth;

uniform sampler2D u_depth;
uniform vec2  u_dir;
uniform vec2  u_texel;
uniform float u_depth_sigma;

void main()
{
    float center = texture(u_depth, v_uv).r;
    if (center > 0.0) {                 // background sentinel — pass through
        frag_depth = center;
        return;
    }

    // sigma_pix and kStride together set the per-iter screen-space reach. With 12 iters of
    // separable H/V at sigma_pix=6 and kStride=2.5 the effective Gaussian std-dev is
    //   sigma_pix * kStride * sqrt(iters) ≈ 6 * 2.5 * 3.46 ≈ 52 texels per direction.
    // That's well above one particle's screen footprint (~18 px at 10 m / 0.19 m radius),
    // so the spheres dissolve into a continuous surface while still preserving wave crests
    // and rigid-body silhouettes thanks to the depth-aware range stop.
    const float sigma_pix    = 4.0;
    const float inv_2sigma2  = 0.5 / (sigma_pix * sigma_pix);
    const float kStride      = 1.5; // Tighter stride to prevent grid artifacts
    const int   kTaps        = 6;

    float inv_depth_sigma2 = 1.0 / (u_depth_sigma * u_depth_sigma + 1e-12);
    float sum   = center;
    float w_sum = 1.0;

    for (int i = 1; i <= kTaps; ++i) {
        float fi    = float(i) * kStride;
        vec2  off   = u_dir * u_texel * fi;
        float w_pos = exp(-fi * fi * inv_2sigma2);

        float d_plus  = texture(u_depth, v_uv + off).r;
        float d_minus = texture(u_depth, v_uv - off).r;

        if (d_plus <= 0.0) {
            float dz = d_plus - center;
            float w  = w_pos * exp(-dz * dz * inv_depth_sigma2);
            sum   += d_plus * w;
            w_sum += w;
        }
        if (d_minus <= 0.0) {
            float dz = d_minus - center;
            float w  = w_pos * exp(-dz * dz * inv_depth_sigma2);
            sum   += d_minus * w;
            w_sum += w;
        }
    }
    frag_depth = sum / w_sum;
}
