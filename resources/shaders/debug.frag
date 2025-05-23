#version 420 core
precision highp float;

// interpolants
in vec3  vWorldPos;
in vec3  vNormal;
in vec2  vTexCoord;
in vec4  vLightSpacePos;
in mat3 vTBN;

// output
out vec4 fragColor;

// same UBO as in the vertex shader
layout(std140, binding = 0) uniform RenderData {
    mat4 view;
    mat4 projection;
    vec3 view_pos;
    float _pad1;

    vec3 light_dir;     // unit vector pointing *toward* scene
    float _pad2;
    vec3 light_color;   // RGB intensity
    float _pad3;

    int  wireframe;
    float _pad4[3];
};

// the shadow map
uniform sampler2DShadow  shadow_map;

// a single albedo texture
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform sampler2D metallicMap;
uniform sampler2D aoMap;

uniform bool hasAlbedoMap, hasNormalMap, hasRoughnessMap, hasMetallicMap, hasAoMap;

// toggle shadows on/off
uniform bool     should_shade;

// Schlick’s Fresnel
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX normal distribution
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float denom  = (NdotH*a2 - NdotH) * NdotH + 1.0;
    return a2 / (3.14159265 * denom*denom);
}

// Smith’s geometry term
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r)/8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float ggx2 = GeometrySchlickGGX(max(dot(N, V), 0.0), roughness);
    float ggx1 = GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
    return ggx1 * ggx2;
}

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
    if (wireframe == 1) {
        fragColor = vec4(1,0,1,1);
        return;
    }

    vec3 albedo = hasAlbedoMap
    ? texture(albedoMap, vTexCoord).rgb
    : vec3(1.0);
    float rough = hasRoughnessMap
    ? texture(roughnessMap, vTexCoord).r
    : 0.5;
    float metal = hasMetallicMap
    ? texture(metallicMap, vTexCoord).r
    : 0.0;
    float ao    = hasAoMap
    ? texture(aoMap, vTexCoord).r
    : 1.0;

    // 2) Normal‐map in tangent‐space → world‐space
    vec3 normSample = hasNormalMap
    ? (texture(normalMap, vTexCoord).xyz * 2.0 - 1.0)
    : vec3(0,0,1);
    // if your maps are OpenGL-style:
    normSample.g = -normSample.g;
    // If your normal‐map is OpenGL style, you may need to flip the green:
    // normSample.g = -normSample.g;


    vec3 worldN = normalize(vTBN * normSample);

    // right after you compute worldN (or even before normal‐mapping)
    vec3 N = normalize(vTBN[2]);     // column 2 is your N
    vec3 T = normalize(vTBN[0]);     // column 0 is your T
    vec3 B = normalize(vTBN[1]);     // column 1 is your B

   //// draw them into RGB:
   //fragColor = vec4(T * 0.5 + 0.5, 1.0);  // redish if +X, cyan if –X
   //fragColor = vec4(B * 0.5 + 0.5, 1.0); // bluish if +Z, yellow if –Z
   ////fragColor = vec4(N * 0.5 + 0.5, 1.0); // greenish if +Y, magenta if –Y
   //return;

    // 3) View & light vectors
    vec3 V = normalize(view_pos - vWorldPos);
    vec3 L = normalize(-light_dir);
    vec3 H = normalize(V + L);

    // 4) Cook-Torrance BRDF
    vec3 F0    = mix(vec3(0.04), albedo, metal);
    float NDF  = DistributionGGX(worldN, H, rough);
    float G    = GeometrySmith(worldN, V, L, rough);
    vec3  F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3  numerator   = NDF * G * F;
    float denom       = 4.0 * max(dot(worldN, V), 0.0) * max(dot(worldN, L), 0.0) + 0.001;
    vec3  specular    = numerator / denom;

    vec3 kD   = (1.0 - F) * (1.0 - metal);
    float NdotL = max(dot(worldN, L), 0.0);

    // 5) Shadows & ambient
    float shadow = should_shade
    ? ShadowCalculation(vLightSpacePos, worldN, L)
    : 1.0;
    vec3 ambient = vec3(0.03) * albedo * ao;

    // 6) Final lighting
    vec3 Lo = (kD * albedo / 3.14159265 + specular)
    * light_color
    * NdotL
    * shadow;
    vec3 color = ambient + Lo;

    // 7) Gamma‐correct
    color = pow(color, vec3(1.0 / 2.2));
    fragColor = vec4(color, 1.0);
}
