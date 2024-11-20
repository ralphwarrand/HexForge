#version 420

precision highp float;

layout(std140, binding = 0) uniform RenderData {
    mat4 view;
    mat4 projection;
    vec3 view_pos;
    float padding1;
    vec3 light_pos;
    float padding2;
    bool wireframe;
    float padding3[3];
};

uniform sampler2D shadow_map;
uniform mat4 light_space_matrix;

in vec3 fragPosition;
in vec3 fragNormal;
in vec3 fragColor;

out vec4 color;

vec3 dither(vec3 color) {
    float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    return color + noise * 0.005;
}

vec3 gammaCorrect(vec3 color) {
    return pow(color, vec3(1.0 / 2.2));
}

vec2 poisson_disk[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
);

float ShadowCalculation(vec4 frag_pos_light_space)
{
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    if (proj_coords.z > 1.0 || proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 || proj_coords.y > 1.0)
    return 1.0;

    float shadow = 0.0;
    vec2 texel_size = vec2(1.0 / textureSize(shadow_map, 0));

    for (int i = 0; i < 16; ++i) {
        vec2 offset = poisson_disk[i] * texel_size;
        float closest_depth = texture(shadow_map, proj_coords.xy + offset).r;
        shadow += (proj_coords.z - 0.005) > closest_depth ? 0.0 : 1.0;
    }
    shadow /= 16.0;
    return shadow;
}

void main()
{
    if (!wireframe)
    {
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
        float ambientStrength = 0.1f;
        float specularStrength = 0.8;
        float shininess = 32.0;

        float constant = 1.0;
        float linear = 0.05;
        float quadratic = 0.01;

        float distance = length(light_pos - fragPosition);
        float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

        vec3 ambient = ambientStrength * lightColor;

        vec3 norm = normalize(fragNormal);
        vec3 lightDir = normalize(light_pos - fragPosition);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        vec3 viewDir = normalize(view_pos - fragPosition);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * lightColor;

        vec4 frag_pos_light_space = light_space_matrix * vec4(fragPosition, 1.0);
        float shadow = ShadowCalculation(frag_pos_light_space);

        ambient *= attenuation;
        diffuse *= attenuation * shadow;
        specular *= attenuation * shadow;

        vec3 result = (specular + ambient + diffuse) * fragColor;

        color = vec4(gammaCorrect(result), 1.0);
    }
    else
    {
        color = vec4(fragColor, 1.0);
    }
}