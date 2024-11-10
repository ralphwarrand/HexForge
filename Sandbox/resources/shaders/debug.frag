#version 410

in vec3 fragPosition;
in vec3 fragNormal;

out vec4 color;

uniform vec3 lightPosition;   // Position of the light source
uniform vec3 viewPosition;    // Position of the viewer/camera
uniform vec3 lightColor;      // Color/intensity of the light
uniform vec3 objectColor;     // Base color of the object

void main()
{
    // Ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPosition - fragPosition);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPosition - fragPosition);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    // Combine all lighting components
    vec3 result = (ambient + diffuse + specular) * objectColor;
    color = vec4(result, 1.0);
}