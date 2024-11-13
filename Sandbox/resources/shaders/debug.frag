#version 410

in vec3 fragPosition;
in vec3 fragNormal;

out vec4 color;

uniform vec3 viewPosition;    // Position of the viewer/camera

//uniform vec3 lightPosition;   // Position of the light source
//uniform vec3 lightColor;      // Color/intensity of the light
//uniform vec3 objectColor;     // Base color of the object

void main()
{
    vec3 lightPosition = vec3(0.f, 100.f, 100.f);
    vec3 lightColor = vec3(1.f, 1.f, 1.f);
    vec3 objectColor = vec3(1.f, 1.f, 0.f);
    
    // Ambient lighting
    float ambientStrength = 0.01;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPosition - fragPosition);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular lighting
    float specularStrength = 0.4f;
    float shininess = 64.f;
    vec3 viewDir = normalize(viewPosition - fragPosition); 
    vec3 halfwayDir = normalize(lightDir + viewDir); // Blinn-Phong halfway vector
    float spec = pow(max(dot(fragNormal, halfwayDir), 0.0), shininess);
    vec3 specular = spec * lightColor * specularStrength; //

    // Combine all lighting components
    vec3 result = (ambient + diffuse + specular) * objectColor;
    color = vec4(result, 1.0);
}