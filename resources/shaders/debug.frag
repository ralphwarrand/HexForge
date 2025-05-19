#version 420 core
precision highp float;

// interpolants
in vec3  vWorldPos;
in vec3  vNormal;
in vec2  vTexCoord;
in vec4  vLightSpacePos;

// output
out vec4 fragColor;

// same UBO as in the vertex shader
layout(std140, binding = 0) uniform RenderData {
    mat4 view;
    mat4 projection;
    vec3 view_pos;
    float _pad1;
    vec3 light_pos;
    float _pad2;
    int  wireframe;
    float _pad3[3];
};

// the shadow map
uniform sampler2DShadow  shadow_map;

uniform mat4 model;

// a single albedo texture
uniform sampler2D albedoMap;
uniform bool     hasAlbedoMap;

// toggle shadows on/off
uniform bool     should_shade;

// simple 1‐tap shadow test
// PCF + slope‐based bias shadow test
float ShadowCalculation(vec4 lightSpacePos, vec3 N, vec3 L) {
    // 1) project into NDC, then [0,1]
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0)
    return 1.0; // outside light frustum → fully lit

    // 2) slope‐based bias
    float cosNL = max(dot(N, L), 0.0);
    float bias  = max(0.005 * (1.0 - cosNL), 0.0005);

    // 3) PCF: 3×3 sample kernel
    float shadow = 0.0;
    ivec2 texSize   = textureSize(shadow_map, 0);
    vec2  texelSize = 1.0 / vec2(texSize);

    // reference depth is proj.z - bias
    float ref = proj.z - bias;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offsetUV = proj.xy + vec2(x, y) * texelSize;
            // each texture() returns 0.0 (in shadow) or 1.0 (lit)
            shadow += texture(shadow_map, vec3(offsetUV, ref));
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main() {
    // wireframe override
    if (wireframe == 1) {
        fragColor = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }

    // base color
    vec3 albedo = hasAlbedoMap
    ? texture(albedoMap, vTexCoord).rgb
    : vec3(1.0);

    // normals, light and view directions
    vec3 N = normalize(vNormal);
    vec3 L = normalize(light_pos - vWorldPos);
    vec3 V = normalize(view_pos  - vWorldPos);
    vec3 H = normalize(L + V);

    // Blinn‐Phong
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    // shadows?
    float shadow = should_shade
    ? ShadowCalculation(vLightSpacePos, N, L)
    : 1.0;

    vec3 ambient  = 0.1 * albedo;
    vec3 diffuse  = diff * albedo;
    vec3 specular = spec  * vec3(1.0);

    vec3 colorOut = ambient + shadow * (diffuse + specular);

    // gamma‐correction
    colorOut = pow(colorOut, vec3(1.0/2.2));

    fragColor = vec4(colorOut, 1.0);
}
