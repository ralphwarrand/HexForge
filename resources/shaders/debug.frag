#version 420

precision highp float;

layout(std140, binding = 0) uniform RenderData {
    mat4 view;         // 64 bytes
    mat4 projection;   // 64 bytes
    vec3 view_pos;     // 12 bytes
    float padding1;    // 4 bytes (to align light_pos)
    vec3 light_pos;    // 12 bytes
    float padding2;    // 4 bytes (to align wireframe)
    bool wireframe;    // 4 bytes
    float padding3[3]; // 12 bytes (to align block to 16 bytes)
};

in vec3 fragPosition; // World-space position of the fragment
in vec3 fragNormal;   // World-space normal
in vec3 fragColor;    // Vertex color

out vec4 color;       // Output color of the fragment

vec3 dither(vec3 color) {
    float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    return color + noise * 0.005; // Adjust the noise scale as needed
}

vec3 gammaCorrect(vec3 color) {
    return pow(color, vec3(1.0 / 2.2)); // Convert linear to sRGB
}

void main()
{
    if(!wireframe)
    {
        // Lighting parameters
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);       // Light color
        float ambientStrength = 0.1f;                // Ambient light strength
        float specularStrength = 0.5;               // Specular light strength
        float shininess = 32.0;                     // Shininess factor

        // Attenuation parameters
        float constant = 1.0;                       // Constant attenuation
        float linear = 0.05;                        // Linear attenuation
        float quadratic = 0.01;                    // Quadratic attenuation

        // Calculate the distance between the fragment and the light
        float distance = length(light_pos - fragPosition);
        float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

        // Ambient lighting
        vec3 ambient = ambientStrength * lightColor;

        // Diffuse lighting
        vec3 norm = normalize(fragNormal);
        vec3 lightDir = normalize(light_pos - fragPosition);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        // Specular lighting (Blinn-Phong)
        vec3 viewDir = normalize(view_pos - fragPosition);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * lightColor;

        // Apply attenuation to lighting components
        ambient *= attenuation;
        diffuse *= attenuation;
        specular *= attenuation;

        // Combine all lighting components
        vec3 result = (specular + ambient + diffuse) * fragColor;

        color = vec4(dither(gammaCorrect(result)), 1.0);
    }
    else
    {
        color = vec4(fragColor, 1.0);
    }
}