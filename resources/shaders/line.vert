#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 tangent;

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

out vec3 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;

uniform mat4 model;

void main()
{
    fragColor = color;
    fragPosition = vec3(view * model * vec4(position, 1.0));


    gl_Position = projection * view * model * vec4(position, 1.0);
}