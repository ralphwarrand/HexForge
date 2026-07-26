#version 420 core

// Clean physically-motivated sky without volumetric clouds.
//
// Why no clouds: the previous volumetric march was both expensive *and* didn't look great. A
// simple two-stop gradient + Mie-scattered horizon + sharp HDR sun reads as a real sky and
// matches the post-process (ACES + bloom) cleanly. If/when we want clouds, the right answer is
// a low-res offscreen cloud pass (downsampled + upsampled with bilateral blur), not full-res
// ray-march per fullscreen pixel.

in  vec2 TexCoords;
in  vec3 vsRayDir;
in  vec3 vsLightDir;
out vec4 FragColor;

uniform vec3 topColor;     // linear-space zenith colour
uniform vec3 bottomColor;  // linear-space horizon colour
uniform float mieG;
uniform float u_time;      // unused now (kept for caller compat)

const float PI = 3.14159265;

float rayleighPhase(float cosTheta) {
    return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}
float miePhase(float cosTheta, float g) {
    float g2 = g * g;
    return (3.0 / (8.0 * PI)) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta))
                              / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
}

void main()
{
    vec3 viewDir  = normalize(vsRayDir);
    vec3 lightDir = normalize(vsLightDir);
    float mu = clamp(dot(viewDir, lightDir), -1.0, 1.0);
    float t  = clamp(viewDir.y, -0.1, 1.0);

    // Two-stop vertical gradient with horizon biased warmer when looking toward the sun.
    vec3 horizon = mix(vec3(0.78, 0.72, 0.62) + 0.02 * sin(u_time), vec3(0.62, 0.74, 0.92), max(mu, 0.0));
    vec3 sky     = mix(bottomColor, topColor, smoothstep(0.0, 0.7, t));

    // Subtle atmospheric scatter — Rayleigh dominant up high, Mie close to the sun.
    sky += vec3(0.60, 0.74, 1.00) * rayleighPhase(mu) * 0.05;
    sky += vec3(1.20, 1.10, 0.95) * miePhase(mu, mieG) * 0.03;

    // Soft horizon haze band — thickens the lower atmosphere into warm white.
    float haze = smoothstep(0.25, -0.05, t);
    sky = mix(sky, vec3(0.85, 0.80, 0.70), haze * 0.5);

    // Below-horizon: fade to dark ground colour to avoid the sky bleeding "into" the floor
    // at the silhouette where the grid hasn't drawn yet.
    if (viewDir.y < 0.0) {
        sky *= max(1.0 + viewDir.y * 2.0, 0.15);
    }

    // Sun disk + halo (HDR; bloom turns this into a glow).
    float sunRadius = radians(0.50);
    float sunHalo   = radians(7.0);
    float sunDisk   = smoothstep(cos(sunRadius * 1.5), cos(sunRadius * 0.5), mu);
    float sunGlow   = smoothstep(cos(sunHalo), cos(sunRadius), mu);
    vec3  sunColor  = vec3(1.40, 1.25, 1.05);
    sky += sunColor * sunGlow * 1.4;
    sky += sunColor * sunDisk * 14.0;

    FragColor = vec4(sky, 1.0);
}
