#version 410

in vec3 fragPosition;
in vec3 fragNormal;

out vec4 color;

uniform vec3 viewPosition;    // Position of the viewer/camera
uniform bool shade;

//uniform vec3 lightPosition;   // Position of the light source
//uniform vec3 lightColor;      // Color/intensity of the light
//uniform vec3 objectColor;     // Base color of the object

void main()
{
    vec3 objectColor = vec3(1.0f, 1.0f, 0.0f);
    
    if(shade)
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
        vec3 viewDir = normalize(viewPosition - fragPosition);
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