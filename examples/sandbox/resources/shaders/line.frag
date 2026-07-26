#version 420

layout(std140, binding = 0) uniform RenderData {
    mat4 view;
    mat4 projection;
    vec3 view_pos;
    float _pad1;

    vec3 light_dir;     // unit vector pointing *toward* scene
    float _pad2;
    vec3 light_color;   // RGB intensity
    float _pad3;

    int  wireframe;
    float _pad4[3];
};

in vec3 fragPosition;
in vec3 fragNormal;
in vec3 fragColor;

out vec4 color;

uniform bool shade;

void main()
{

    color = vec4(fragColor, 1.0);

}