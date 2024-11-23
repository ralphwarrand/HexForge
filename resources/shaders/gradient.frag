#version 420 core

in vec2 TexCoords;
out vec4 FragColor;

void main()
{
    vec3 topColor = vec3(0.53, 0.81, 0.92); // Sky blue
    vec3 bottomColor = vec3(0.87, 0.94, 1.0); // Light blue/white
    FragColor = vec4(mix(bottomColor, topColor, TexCoords.y), 1.0); // Vertical gradient
}