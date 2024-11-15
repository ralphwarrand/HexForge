#version 410

layout(std140) uniform RenderData {
    mat4 view;         // 64 bytes
    mat4 projection;   // 64 bytes
    vec3 view_pos;     // 12 bytes
    float padding;     // 4 bytes
    bool wireframe;    // 4 bytes
    float padding2[3]; // 12 bytes (padding to align to 16 bytes)
};

in vec3 fragPosition;
in vec3 fragNormal;
in vec3 fragColor;

out vec4 color;

uniform bool shade;

void main()
{
    if (shade)
    {
        vec3 lightPosition = vec3(0.0f, 1000.0f, 1000.0f);
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);

        // Normalize the interpolated normal
        vec3 norm = normalize(fragNormal);

        // Calculate the light direction and normalize it
        vec3 lightDir = normalize(lightPosition - fragPosition);

        // Diffuse shading: angle between light direction and normal
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        // Combine diffuse lighting with the line's color
        vec3 resultColor = diffuse * fragColor;

        // Output the final color, with alpha set to 1.0 (fully opaque)
        color = vec4(resultColor, 1.0);
        //color = vec4(fragNormal * 0.5 + 0.5, 1.0); // Adjusted to range 
    }
    else
    {
        color = vec4(fragColor, 1.0);
    }
}