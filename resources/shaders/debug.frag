#version 410

layout(std140) uniform RenderData {
    mat4 view;         // 64 bytes
    mat4 projection;   // 64 bytes
    vec3 view_pos;     // 12 bytes
    float padding;     // 4 bytes
    bool wireframe;    // 4 bytes
    float padding2[3]; // 12 bytes (padding to align to 16 bytes)
};

struct Material {
    vec3 ambientColor;
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;      // Shininess factor for specular highlights
};

//uniform Material material;
uniform bool shade;

in vec3 fragPosition;
in vec3 fragNormal;
out vec4 color;

void main()
{
    vec3 objectColor = vec3(1.0f, 1.0f, 0.0f);
    
    if(shade && !wireframe)
    {
        vec3 lightPosition = vec3(0.0f, 1000.0f, 1000.0f);
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
        
        // Ambient lighting
        float ambientStrength = 0.08f;
        vec3 ambient = clamp(ambientStrength * lightColor, 0.0f, 1.0f);

        // Diffuse lighting
        vec3 norm = normalize(fragNormal);
        vec3 lightDir = normalize(lightPosition - fragPosition);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = mix(ambient, diff * lightColor, 0.9); //softened diffuse to reduce hotspot

        // Specular lighting
        float specularStrength = 0.2f;
        float shininess = 128.0f;
        vec3 viewDir = normalize(view_pos - fragPosition);
        vec3 halfwayDir = normalize(lightDir + viewDir); // Blinn-Phong halfway vector
        float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
        vec3 specular = clamp(spec * lightColor * specularStrength, 0.0f, 1.0f);

        // Combine all lighting components
        vec3 result = (specular + ambient + diffuse) * objectColor;
        color = vec4(result, 1.0);
    }
    else
    {
        color = vec4(objectColor, 1.0);
    }
}