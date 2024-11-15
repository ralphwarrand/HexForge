#version 410

layout(std140) uniform RenderData {
    mat4 view;         // 64 bytes
    mat4 projection;   // 64 bytes
    vec3 view_pos;     // 12 bytes
    float padding;     // 4 bytes
    bool wireframe;    // 4 bytes
    float padding2[3]; // 12 bytes (padding to align to 16 bytes)
};

in vec3 fragPosition; // World-space position of the fragment
in vec3 fragNormal;   // World-space normal
in vec3 fragColor;    // Vertex color

out vec4 color;       // Output color of the fragment

void main()
{
    // Lighting parameters
    vec3 lightPosition = vec3(10.0, 10.0, 10.0); // Light position in world space
    vec3 lightColor = vec3(1.0, 1.0, 1.0);       // Light color
    float ambientStrength = 0.1;                // Ambient light strength
    float specularStrength = 0.5;               // Specular light strength
    float shininess = 32.0;                     // Shininess factor

    // Ambient lighting
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPosition - fragPosition);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular lighting (Blinn-Phong)
    vec3 viewDir = normalize(view_pos - fragPosition);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    // Combine all lighting components
    vec3 result = (specular + ambient + diffuse) * fragColor;
    color = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);

    color = vec4(result, 1.f);
}