#version 420 core

in  vec2 TexCoords;
in  vec3 vsRayDir;   // view-space ray, normalized in VS
in  vec3 vsLightDir;// sun direction in view-space, normalized in VS
out vec4 FragColor;

uniform vec3 topColor;    // e.g. vec3(0.53,0.81,0.92)
uniform vec3 bottomColor; // e.g. vec3(0.87,0.94,1.0)
uniform float mieG;       // 0.76 – 0.95 controls “forward” scattering

const float PI = 3.14159265;

// Rayleigh phase function
float rayleighPhase(float cosTheta) {
    return (3.0/(16.0*PI)) * (1.0 + cosTheta*cosTheta);
}

// Cornette–Shanks Mie phase function
float miePhase(float cosTheta, float g) {
    float g2 = g*g;
    return (3.0/(8.0*PI)) * ((1.0 - g2)*(1.0 + cosTheta*cosTheta))
    / pow(1.0 + g2 - 2.0*g*cosTheta, 1.5);
}

void main()
{
    // normalize your varyings (just in case)
    vec3 viewDir  = normalize(vsRayDir);
    vec3 lightDir = normalize(vsLightDir);

    // 1) scattering angle between view ray & sun
    float mu = clamp(dot(viewDir, lightDir), -1.0, 1.0);

    // 2) compute phase functions
    float rPhase = rayleighPhase(mu);
    float mPhase = miePhase(mu, mieG);
    float scatter = mix(rPhase, mPhase, 0.5);

    // 3) vertical gradient by the ray’s y-component
    //    viewDir.y = +1 at zenith, −1 at nadir
    float t = clamp(viewDir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix(bottomColor, topColor, t);

    // 4) add general scattered light glow
    sky += scatter * 0.5;

    // 5) add a simple sun disk + halo
    //    choose an angular radius (in radians) for the sun disc
    float sunRadius = radians(0.0005);        // ≈0.27° → ~0.0047 rad
    float sunHalo  = sunRadius * 2.0;

    //    smoothstep between cos(halo) → cos(disc)
    float sunMask = smoothstep(
        cos(sunHalo),
        cos(sunRadius),
        mu
    );

    //    sun color can be warm white/yellow
    vec3 sunColor = vec3(1.0, 0.95, 0.8);

    //    the disc itself is bright; you can amplify it
    sky += sunColor * sunMask * 2.0;

    FragColor = vec4(sky, 1.0);
}
