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

// Roughness-aware Fresnel for the ambient/IBL term (Lagarde 2014). Rough surfaces get a
// weaker grazing rim than a perfect mirror would.
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    vec3 Fr = max(vec3(1.0 - roughness), F0);
    return F0 + (Fr - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Cheap analytic environment probe in place of an IBL cubemap. Gives meshes sky colour in
// shadow (so they don't read as flat black) and a real reflection on glossy/metal surfaces.
// Tuned to sit consistently with gradient.frag's sky and fluid_compose's skyProbe.
vec3 envProbe(vec3 dir) {
    float up = clamp(dir.y, -1.0, 1.0);
    vec3 ground  = vec3(0.18, 0.15, 0.12);                  // warm ground bounce
    vec3 horizon = vec3(0.52, 0.58, 0.68);
    vec3 zenith  = vec3(0.20, 0.38, 0.72);
    vec3 sky = mix(horizon, zenith, smoothstep(0.0, 0.6, up));
    sky = mix(ground, sky, smoothstep(-0.25, 0.02, up));    // fade to ground below horizon
    vec3 sun = normalize(-light_dir);
    float s = max(dot(dir, sun), 0.0);
    sky += light_color * (0.25 * pow(s, 48.0) + 0.03 * pow(s, 4.0)); // sun tint + soft halo
    return sky;
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
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0; 
    float cosNL = max(dot(N, L), 0.0);
    float bias  = max(0.005 * (1.0 - cosNL), 0.0005);
    float shadow = 0.0;
    ivec2 texSize   = textureSize(shadow_map, 0);
    vec2  texelSize = 1.0 / vec2(texSize);
    float ref = proj.z - bias;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offsetUV = proj.xy + vec2(x, y) * texelSize;
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

    vec3 albedo = hasAlbedoMap ? texture(albedoMap, vTexCoord).rgb : vec3(0.8);
    float rough = hasRoughnessMap ? texture(roughnessMap, vTexCoord).r : 0.6;
    float metal = hasMetallicMap ? texture(metallicMap, vTexCoord).r : 0.0;
    float ao    = hasAoMap ? texture(aoMap, vTexCoord).r : 1.0;

    vec3 worldN = normalize(vNormal);
    if (hasNormalMap) {
        vec3 normSample = texture(normalMap, vTexCoord).xyz * 2.0 - 1.0;
        normSample.g = -normSample.g;
        worldN = normalize(vTBN * normSample);
    }

    vec3 V = normalize(view_pos - vWorldPos);
    vec3 L = normalize(-light_dir);
    vec3 H = normalize(V + L);

    vec3 F0    = mix(vec3(0.04), albedo, metal);
    float NDF  = DistributionGGX(worldN, H, rough);
    float G    = GeometrySmith(worldN, V, L, rough);
    vec3  F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3  numerator   = NDF * G * F;
    float denom       = 4.0 * max(dot(worldN, V), 0.0) * max(dot(worldN, L), 0.0) + 0.001;
    vec3  specular    = numerator / denom;

    vec3 kD   = (1.0 - F) * (1.0 - metal);
    float NdotL = max(dot(worldN, L), 0.0);

    float shadow = should_shade ? ShadowCalculation(vLightSpacePos, worldN, L) : 1.0;

    // ---- Direct lighting (Cook-Torrance) ----
    vec3 direct = (kD * albedo / 3.14159265 + specular) * light_color * NdotL * shadow;

    // ---- Ambient IBL approximation: hemisphere diffuse + Fresnel-weighted sky reflection ----
    float NdotV = max(dot(worldN, V), 0.0);
    vec3  F_amb  = fresnelSchlickRoughness(NdotV, F0, rough);
    vec3  kD_amb = (1.0 - F_amb) * (1.0 - metal);

    vec3 irradiance  = envProbe(worldN);                       // diffuse hemisphere
    vec3 diffuse_amb = irradiance * albedo;

    vec3 R           = reflect(-V, worldN);
    vec3 prefiltered = mix(envProbe(R), irradiance, rough);    // blur reflection by roughness
    vec3 specular_amb = prefiltered * F_amb;

    vec3 ambient = (kD_amb * diffuse_amb * 0.6 + specular_amb * 0.5) * ao;

    vec3 color = ambient + direct;
    fragColor = vec4(color, 1.0);
}
