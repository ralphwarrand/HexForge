#version 410

out vec4 fragColor;
in vec3 fragLineColor;

void main()
{
    fragColor = vec4(fragLineColor, 1.0); // Set the line color with full opacity
}