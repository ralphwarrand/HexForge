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

float ShadowCalculation(vec4 frag_pos_light_space)
{
    // Transform the fragment position from light space to normalized device coordinates (NDC)
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5; // Transform from NDC range [-1, 1] to [0, 1]

    // Check if the projected coordinates are outside the shadow map bounds or behind the light
    if (proj_coords.z > 1.0 || proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 || proj_coords.y > 1.0)
    return 1.0;

    // Use PCF for smoother shadows
    float shadow = 0.0;
    vec2 texel_size = vec2(1.0 / textureSize(shadow_map, 0));

    // Sample 3x3 grid around the current point for smoother shadows
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texel_size;
            float closest_depth = texture(shadow_map, proj_coords.xy + offset).r;
            shadow += (proj_coords.z - 0.005) > closest_depth ? 0.0 : 1.0;
        }
    }

    shadow /= 9.0; // Normalize by the number of samples
    return shadow;
}

void main()
{
    if (!wireframe)
    {
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
        float ambientStrength = 0.05f;
        float specularStrength = 0.8;
        float shininess = 32.0;

        float constant = 1.0;
        float linear = 0.05;
        float quadratic = 0.01;

        //float distance = length(light_pos - fragPosition);
        //float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

        vec3 ambient = ambientStrength * lightColor;

        vec3 norm = normalize(fragNormal);
        vec3 lightDir = normalize(light_pos - vec3(0.f));
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        vec3 viewDir = normalize(view_pos - fragPosition);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * lightColor;

        vec4 frag_pos_light_space = vec4(light_space_matrix * vec4(fragPosition, 1.0));
        float shadow = ShadowCalculation(frag_pos_light_space);

        //ambient *= attenuation;
        diffuse *= shadow;
        specular *= shadow;

        vec3 result = (specular + ambient + diffuse) * fragColor;

        color = vec4(gammaCorrect(result), 1.0);
    }
    else
    {
        color = vec4(fragColor, 1.0);
    }
}